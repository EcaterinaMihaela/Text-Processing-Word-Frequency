#include <mpi.h>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cctype>
#include <string>

using namespace std;

// comparare sortare
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

    string full_text;
    vector<char> text_buffer;
    vector<int> sendcounts(size), displs(size);

    int total_size = 0;

    //DOAR procesul 0 citește
    if (rank == 0)
    {
        ifstream file("D:/ADP/ProiectAPD/VariantaSecventiala/Alice_in_Wonderland_AllChapters.txt");

        if (!file)
        {
            cout << "Eroare fisier\n";
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        file.seekg(0, ios::end);
        total_size = file.tellg();
        file.seekg(0, ios::beg);

        text_buffer.resize(total_size);
        file.read(text_buffer.data(), total_size);

        // împărțire
        int base = total_size / size;
        int remainder = total_size % size;

        int offset = 0;
        for (int i = 0; i < size; i++)
        {
            sendcounts[i] = base + (i < remainder ? 1 : 0);
            displs[i] = offset;
            offset += sendcounts[i];
        }
    }

    MPI_Bcast(&total_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(sendcounts.data(), size, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(displs.data(), size, MPI_INT, 0, MPI_COMM_WORLD);

    vector<char> local_buffer(sendcounts[rank]);

    MPI_Scatterv(
        text_buffer.data(),
        sendcounts.data(),
        displs.data(),
        MPI_CHAR,
        local_buffer.data(),
        sendcounts[rank],
        MPI_CHAR,
        0,
        MPI_COMM_WORLD
    );

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    // procesare locală
    unordered_map<string, int> local_freq;
    local_freq.reserve(5000);

    string word;

    for (char c : local_buffer)
    {
        unsigned char uc = (unsigned char)c;

        if (isalpha(uc))
        {
            word += tolower(uc);
        }
        else
        {
            if (!word.empty())
            {
                local_freq[word]++;
                word.clear();
            }
        }
    }
    if (!word.empty())
        local_freq[word]++;

    // serializare RAPIDĂ (fără stringstream)
    string serialized;
    serialized.reserve(local_freq.size() * 10);

	//serializează map ul ca string -"cuvânt frecvență"
    for (auto& p : local_freq)
    {
        serialized += p.first;
        serialized += " ";
        serialized += to_string(p.second);
        serialized += "\n";
    }

    int local_size = serialized.size();

    vector<int> recv_sizes(size);
    MPI_Gather(&local_size, 1, MPI_INT, recv_sizes.data(), 1, MPI_INT, 0, MPI_COMM_WORLD);

    vector<int> recv_displs(size);
    string global_buffer;

    if (rank == 0)
    {
        int offset = 0;
        for (int i = 0; i < size; i++)
        {
            recv_displs[i] = offset;
            offset += recv_sizes[i];
        }
        global_buffer.resize(offset);
    }

    MPI_Gatherv(
        serialized.data(),
        local_size,
        MPI_CHAR,
        rank == 0 ? &global_buffer[0] : nullptr,
        recv_sizes.data(),
        recv_displs.data(),
        MPI_CHAR,
        0,
        MPI_COMM_WORLD
    );

    // procesul 0 reconstruiește
    if (rank == 0)
    {
        unordered_map<string, int> global_freq;
        global_freq.reserve(10000);

        string word;
        int count;

        size_t i = 0;
        while (i < global_buffer.size())
        {
            word.clear();

            while (i < global_buffer.size() && global_buffer[i] != ' ')
                word += global_buffer[i++];

            i++; // skip space

            count = 0;
            while (i < global_buffer.size() && global_buffer[i] != '\n')
                count = count * 10 + (global_buffer[i++] - '0');

            i++; // skip newline

            global_freq[word] += count;
        }

        vector<pair<string, int>> words(global_freq.begin(), global_freq.end());

        //top 10 cuv frecv 
        int k = min(10, (int)words.size());
        partial_sort(words.begin(), words.begin() + k, words.end(), cmp);

        cout << "Top 10 cuvinte:\n";
        for (int i = 0; i < k; i++)
        {
            cout << words[i].first << " : " << words[i].second << endl;
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double end = MPI_Wtime();

    if (rank == 0)
    {
        cout << "\nTimp MPI FINAL: " << (end - start) * 1000 << " ms\n";
    }

    MPI_Finalize();
    return 0;
}