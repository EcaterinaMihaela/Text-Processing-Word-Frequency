#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <chrono>
#include <omp.h> // OpenMP

using namespace std;
using namespace std::chrono;

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

bool cmp(const pair<string, int>& a, const pair<string, int>& b)
{
    return a.second > b.second;
}

int main()
{
    ifstream file("D:/ADP/ProiectAPD/VariantaParalela_OpenMP/Alice_in_Wonderland_chapter1.txt");

    if (!file)
    {
        cout << "Eroare la deschiderea fisierului\n";
        return 1;
    }

    if (file.peek() == ifstream::traits_type::eof()) {
        cout << "Fisierul este gol!\n";
        return 1;
    }

    vector<string> allWords;
    string word;

    // Citim toate cuvintele intr-un vector (secvential)
    while (file >> word)
    {
        word = normalize(word);
        if (word != "")
            allWords.push_back(word);
    }

    file.close();

    auto start = high_resolution_clock::now();

    int num_threads = omp_get_max_threads();

    // vector de map-uri (cate unul per thread)
    vector<unordered_map<string, int>> local_maps(num_threads);

    // Paralelizare
#pragma omp parallel
    {
        int thread_id = omp_get_thread_num();

#pragma omp for
        for (int i = 0; i < allWords.size(); i++)
        {
			local_maps[thread_id][allWords[i]]++;   //fiecare fir scrie in dreptul lui(fara race condition)
        }
    }

	// Reducere manuala-combinare rezultate intr-un map global
    unordered_map<string, int> frequency;

    for (int i = 0; i < num_threads; i++)
    {
        for (auto& pair : local_maps[i])
        {
            frequency[pair.first] += pair.second;
        }
    }

    // Sortare
    vector<pair<string, int>> words(frequency.begin(), frequency.end());
    sort(words.begin(), words.end(), cmp);

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start);

    int N = 10;

    cout << "Cele mai frecvente " << N << " cuvinte:\n";

    for (int i = 0; i < N && i < words.size(); i++)
    {
        cout << words[i].first << " : " << words[i].second << endl;
    }

    cout << "\nTimp total de procesare: " << duration.count() << " ms\n";

    return 0;
}