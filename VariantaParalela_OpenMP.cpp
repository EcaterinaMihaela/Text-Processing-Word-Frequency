#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <omp.h>

using namespace std;
using namespace std::chrono;

int main()
{
    auto t_start = high_resolution_clock::now();

    // citire in buffer binar
    ifstream file("D:/ADP/ProiectAPD/VariantaSecventiala/Alice_in_Wonderland_chapter1.txt",
        ios::binary | ios::ate);
    if (!file) { cout << "Eroare la deschidere\n"; return 1; }

    // tellg() ne da pozitia curenta = dimensiunea fisierului
    size_t file_size = file.tellg();
    file.seekg(0); //ne intoarcem la inceputul fisierului

    // Citim tot fisierul intr-un singur buffer
    vector<char> buf(file_size + 1, '\0');
    file.read(buf.data(), file_size);
    file.close();

    //IMPARTIRE BUFFER PE THREAD-URI (fara allWords vector!)
    // fiecare thread isi calculeaza singur chunk-ul din buffer
    // si numara cuvintele direct, fara vector intermediar de string-uri
    int num_threads = omp_get_max_threads();

    // un map local per thread ca sa evitam race condition-urile
    vector<unordered_map<string, int>> local_maps(num_threads);

#pragma omp parallel num_threads(num_threads)
    {
        int tid = omp_get_thread_num(); //id-ul thread-ului curent
        int nth = omp_get_num_threads();    //numarul total de thread-uri active

        // calculam chunk-ul fiecarui thread
        size_t chunk = file_size / nth;
        size_t start = tid * chunk;
        size_t end = (tid == nth - 1) ? file_size : start + chunk;

        // ajustam start-ul: daca incepem in mijlocul unui cuvant
        // il sarim ca sa nu il numaram de doua ori cu thread-ul anterior
        if (tid != 0) {
            // sarim literele ramase din cuvantul anterior
            while (start < file_size && isalpha((unsigned char)buf[start]))
                start++;
            // sarim separatorii pana la primul cuvant complet al nostru
            while (start < file_size && !isalpha((unsigned char)buf[start]))
                start++;
        }

        // ajustam end-ul: daca granita cade in mijlocul unui cuvant
        // dam inapoi ca sa il lasam thread-ului urmator
        if (tid != nth - 1) {
            while (end > start && isalpha((unsigned char)buf[end]))
                end--;
        }

        // Fiecare thread parcurge chunk-ul sau si numara caracetr cu caracter
        string word;
        word.reserve(32);  // pre-alocam spatiu ca sa evitam realocari frecvente
        auto& mymap = local_maps[tid]; // referinta la map-ul local al acestui thread

        for (size_t i = start; i < end; i++) {
            unsigned char c = (unsigned char)buf[i];
            if (isalpha(c)) {
                // litera: o adaugam la cuvantul curent in lowercase
                word += tolower(c);
            }
            else if (!word.empty()) {
                // separator: salvam cuvantul construit si il resetam
                mymap[word]++;
                word.clear();
            }
        }
        // salvam ultimul cuvant daca chunk-ul nu se termina cu separator
        if (!word.empty())
            mymap[word]++;
    }

    // REDUCERE: combinam local_maps intr-un map global
    unordered_map<string, int> frequency;
    frequency.reserve(1 << 17); // pre-alocam ~131k bucket-uri

    for (int i = 0; i < num_threads; i++)
        for (auto& [w, c] : local_maps[i])
            frequency[w] += c;

    //SORTARE SI AFISARE
    vector<pair<string, int>> words(frequency.begin(), frequency.end());
    sort(words.begin(), words.end(),
        [](auto& a, auto& b) { return a.second > b.second; });

    auto t_end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(t_end - t_start);

    cout << "Cele mai frecvente 10 cuvinte:\n";
    for (int i = 0; i < 10 && i < (int)words.size(); i++)
        cout << words[i].first << " : " << words[i].second << "\n";

    cout << "\nTimp total: " << duration.count() << " ms\n";
    cout << "Threads folosite: " << num_threads << "\n";

    return 0;
}