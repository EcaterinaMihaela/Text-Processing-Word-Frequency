#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <thread>
#include <cctype>
#include <chrono>

using namespace std;
using namespace std::chrono;

// normalizare
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

// comparare pentru sortare
bool cmp(const pair<string, int>& a, const pair<string, int>& b)
{
    return a.second > b.second;
}

// functie executata de fiecare thread
void process_chunk(const vector<string>& words,
    int start, int end,
    unordered_map<string, int>& local_freq)
{
    for (int i = start; i < end; i++)
    {
        local_freq[words[i]]++;
    }
}

int main()
{
    ifstream file("D:/ADP/ProiectAPD/VariantaSecventiala/Alice_in_Wonderland_AllChapters.txt");

    if (!file)
    {
        cout << "Eroare la fisier\n";
        return 1;
    }

    vector<string> allWords;
    string word;

    //citire + normalizare
    while (file >> word)
    {
        word = normalize(word);
        if (word != "")
            allWords.push_back(word);
    }

    file.close();

    auto start_time = high_resolution_clock::now();

    //numar thread-uri
    int num_threads = thread::hardware_concurrency();

    vector<thread> threads;
    vector<unordered_map<string, int>> local_maps(num_threads);

    int chunk_size = allWords.size() / num_threads;

    //creare thread-uri
    for (int i = 0; i < num_threads; i++)
    {
        int start = i * chunk_size;
        int end = (i == num_threads - 1) ? allWords.size() : start + chunk_size;

		//porneste firul si calc frecv locala cu fctia process_chunk
        threads.emplace_back(process_chunk,
			cref(allWords),     ///trimite vectorul normaliz ca ref constanta
            start, end,         //intervalul
			ref(local_maps[i]));    //trimite map-ul local ca referinta
    }

    //join thread-uri
    for (auto& t : threads)
        t.join();

    //reducere
    unordered_map<string, int> global_freq;

    for (int i = 0; i < num_threads; i++)
    {
        for (auto& p : local_maps[i])
        {
            global_freq[p.first] += p.second;
        }
    }

    //sortare
    vector<pair<string, int>> words(global_freq.begin(), global_freq.end());
    sort(words.begin(), words.end(), cmp);

    auto end_time = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end_time - start_time);

    cout << "Top 10 cuvinte:\n";
    for (int i = 0; i < 10 && i < words.size(); i++)
    {
        cout << words[i].first << " : " << words[i].second << endl;
    }

    cout << "\nTimp Threads: " << duration.count() << " ms\n";

    return 0;
}