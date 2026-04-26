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

void process_chunk(const char* buf, size_t start, size_t end,
    unordered_map<string, int>& local_freq)
{
    string word;
    word.reserve(32);

    for (size_t i = start; i < end; i++) {
        unsigned char c = (unsigned char)buf[i];
        if (isalpha(c)) {
            word += tolower(c);
        }
        else if (!word.empty()) {
            local_freq[word]++;
            word.clear();
        }
    }
    if (!word.empty())
        local_freq[word]++;
}

int main()
{
    auto t_start = high_resolution_clock::now();

    // CITIRE INTEGRALA IN BUFFER BINAR
    ifstream file("D:/ADP/ProiectAPD/VariantaSecventiala/Alice_in_Wonderland_x30mb.txt",
        ios::binary | ios::ate);
    if (!file) { cout << "Eroare la fisier\n"; return 1; }

    size_t file_size = file.tellg();
    file.seekg(0);

    vector<char> buf(file_size + 1, '\0');
    file.read(buf.data(), file_size);
    file.close();

    // IMPARTIRE PE THREAD-URI CU GRANITE CORECTE
    int num_threads = (int)thread::hardware_concurrency();

    vector<size_t> starts(num_threads), ends(num_threads);

    for (int i = 0; i < num_threads; i++) {
        size_t s = (size_t)i * file_size / num_threads;
        size_t e = (size_t)(i + 1) * file_size / num_threads;

        // Ajusteaza start: sari cuvantul partial de la inceput
        if (i != 0) {
            while (s < file_size && isalpha((unsigned char)buf[s])) s++;
            while (s < file_size && !isalpha((unsigned char)buf[s])) s++;
        }

        // Ajusteaza end: daca granita e in mijlocul unui cuvant, da inapoi
        if (i != num_threads - 1) {
            while (e > s && isalpha((unsigned char)buf[e])) e--;
        }
        else {
            e = file_size;
        }

        starts[i] = s;
        ends[i] = e;
    }

    // LANSARE THREAD-URI
    vector<thread> threads;
    vector<unordered_map<string, int>> local_maps(num_threads);

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(process_chunk,
            buf.data(), starts[i], ends[i],
            ref(local_maps[i]));
    }

    for (auto& t : threads) t.join();

    // REDUCERE
    unordered_map<string, int> global_freq;
    global_freq.reserve(1 << 17);

    for (int i = 0; i < num_threads; i++)
        for (auto& [w, c] : local_maps[i])
            global_freq[w] += c;

    // SORTARE SI AFISARE
    vector<pair<string, int>> words(global_freq.begin(), global_freq.end());
    sort(words.begin(), words.end(),
        [](auto& a, auto& b) { return a.second > b.second; });

    auto t_end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(t_end - t_start);

    cout << "Cele mai frecvente 10 cuvinte:\n";
    for (int i = 0; i < 10 && i < (int)words.size(); i++)
        cout << words[i].first << " : " << words[i].second << "\n";

    cout << "\nTimp Threads: " << duration.count() << " ms\n";
    cout << "Threads folosite: " << num_threads << "\n";

    return 0;
}