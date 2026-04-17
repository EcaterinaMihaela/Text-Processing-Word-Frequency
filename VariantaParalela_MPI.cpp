#include <mpi.h>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <sstream>
#include <cctype>

using namespace std;

// normalizare cuvant
string normalize(const string& word)
{
    string result;
    for (unsigned char c : word)
    {
        if (isalpha(c))
            result += tolower(c);
    }
    return result;
}

// comparator
bool cmp(const pair<string, int>& a, const pair<string, int>& b)
{
    return a.second > b.second;
}

int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    vector<string> allWords;

	MPI_Barrier(MPI_COMM_WORLD);
	double start = MPI_Wtime();

    //doar rank 0 citeste
    if (rank == 0)
    {
        ifstream file("D:/ADP/ProiectAPD/VariantaSecventiala/Alice_in_Wonderland_x100.txt");

        if (!file)
        {
            cout << "Eroare fisier\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        string word;
        while (file >> word)
        {
            word = normalize(word);
            if (!word.empty())
                allWords.push_back(word);
        }
    }

    // trimite numarul total de cuvinte
    int total_words = allWords.size();
    MPI_Bcast(&total_words, 1, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> sendcounts(size), displs(size);  //cat primeste fiecare proces si de unde incepe fiecare bucata

    //impartire
    int base = total_words / size;
    int remainder = total_words % size;

    //pt fiec proces :ia bucata asta incepand de aici
    int offset = 0;
    for (int i = 0; i < size; i++)
    {
        sendcounts[i] = base + (i < remainder ? 1 : 0); //unele procese primesc +1 caracter
        displs[i] = offset;
        offset += sendcounts[i];
    }

	//fiecare proces primeste nr de cuvinte din bucata lui
    int local_n = sendcounts[rank];

    vector<string> local_words(local_n);



	//serializare pe rank 0 din string in char pentru MPI_Scatterv
    vector<char> send_buffer;
    vector<int> word_sizes;

    if (rank == 0)
    {
        for (auto& w : allWords)
        {
            word_sizes.push_back(w.size());
            send_buffer.insert(send_buffer.end(), w.begin(), w.end());
        }
    }


    // fiec proces primeste dimensiunile cuvintelor lui
    vector<int> local_word_sizes(local_n);
    MPI_Scatterv(word_sizes.data(), sendcounts.data(), displs.data(),
        MPI_INT, local_word_sizes.data(), local_n,
        MPI_INT, 0, MPI_COMM_WORLD);

    // calculam offset pentru caractere
    //cate carac treb trimise fiec proces
    vector<int> char_counts(size), char_displs(size);

    if (rank == 0)
    {
        int char_offset = 0;
        for (int i = 0; i < size; i++)
        {
            char_counts[i] = 0;
            for (int j = displs[i]; j < displs[i] + sendcounts[i]; j++)
                char_counts[i] += word_sizes[j];

            char_displs[i] = char_offset;
            char_offset += char_counts[i];
        }
    }


    //fiec proces primeste bucata lui de text
    int local_char_count = 0;
    for (int s : local_word_sizes)
        local_char_count += s;

    vector<char> local_chars(local_char_count);

    MPI_Scatterv(send_buffer.data(), char_counts.data(), char_displs.data(),
        MPI_CHAR, local_chars.data(), local_char_count,
        MPI_CHAR, 0, MPI_COMM_WORLD);

	// reconstruire cuvinte locale in stringuri
    int pos = 0;
    for (int i = 0; i < local_n; i++)
    {
        string w(local_chars.begin() + pos,
            local_chars.begin() + pos + local_word_sizes[i]);
        local_words[i] = w;
        pos += local_word_sizes[i];
    }

    

    //frecventa locala
    unordered_map<string, int> local_freq;

    for (auto& w : local_words)
        local_freq[w]++;

    //serializare rezultate
    string serialized;
    for (auto& p : local_freq)
    {
        serialized += p.first + " " + to_string(p.second) + "\n";
    }

    int local_size = serialized.size();

	//primeste rank 0 cat a calculat fiec proces
    vector<int> recv_sizes(size);
    MPI_Gather(&local_size, 1, MPI_INT, recv_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> recv_displs(size);
    string global_buffer;

	//calculam displs pentru rezultate
    if (rank == 0)
    {
        int offset2 = 0;
        for (int i = 0; i < size; i++)
        {
            recv_displs[i] = offset2;
            offset2 += recv_sizes[i];
        }
        global_buffer.resize(offset2);
    }

    MPI_Gatherv(serialized.data(), local_size, MPI_CHAR,
        rank == 0 ? &global_buffer[0] : nullptr,
        recv_sizes.data(), recv_displs.data(),
        MPI_CHAR, 0, MPI_COMM_WORLD);

	//combinare rezultate si sortare
    if (rank == 0)
    {
        unordered_map<string, int> global_freq;

        string word;
        int count;
        size_t i = 0;

        while (i < global_buffer.size())
        {
            word.clear();

            while (i < global_buffer.size() && global_buffer[i] != ' ')
                word += global_buffer[i++];

            i++;

            count = 0;
            while (i < global_buffer.size() && global_buffer[i] != '\n')
                count = count * 10 + (global_buffer[i++] - '0');

            i++;

            global_freq[word] += count;
        }

        vector<pair<string, int>> words(global_freq.begin(), global_freq.end());

        int k = min(10, (int)words.size());
        partial_sort(words.begin(), words.begin() + k, words.end(), cmp);

        cout << "Top 10 cuvinte:\n";
        for (int i = 0; i < k; i++)
            cout << words[i].first << " : " << words[i].second << endl;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double end = MPI_Wtime();

    if (rank == 0)
        cout << "\nTimp MPI: " << (end - start) * 1000 << " ms\n";

    MPI_Finalize();
    return 0;
}