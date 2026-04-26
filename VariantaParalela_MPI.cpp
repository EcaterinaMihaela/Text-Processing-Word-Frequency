#include <mpi.h>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cctype>
#include <string>

using namespace std;

//// transforma un caracter in litera mica sau returneaza '\0' daca nu e litera
inline char normalize_char(char c) {
    if (isalpha((unsigned char)c))
        return tolower((unsigned char)c);
    return '\0';
}

/// adauga cuvantul curent in map si il reseteaza
void add_word(unordered_map<string, int>& freq, string& word) {
    if (!word.empty()) {
        freq[word]++;
        word.clear();
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    MPI_File file;
    const char* filename = "D:/ADP/ProiectAPD/VariantaSecventiala/Alice_in_Wonderland_x30mb.txt";

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    //// deschidem fisierul in mod doar citire, toti procesii il acceseaza in paralel
    MPI_File_open(MPI_COMM_WORLD, filename,
        MPI_MODE_RDONLY, MPI_INFO_NULL, &file);

    //// aflam dimensiunea totala a fisierului
    MPI_Offset file_size;
    MPI_File_get_size(file, &file_size);

    // impartim fisierul in chunk-uri egale, cate unul per proces
    MPI_Offset chunk = file_size / size;
    // fiecare proces isi calculeaza offsetul de start
    MPI_Offset read_start = rank * chunk;
    // ultimul proces ia si restul ramas din impartire
    MPI_Offset read_end = (rank == size - 1) ? file_size : read_start + chunk;

    // Citim 200bytes la dreapta ca sa detectam
    // daca ultimul cuvant al chunk e complet sau taiat
    const MPI_Offset EXTRA = 200;
    MPI_Offset actual_read_end = (rank == size - 1)
        ? file_size
        : min(file_size, read_end + EXTRA);

    MPI_Offset read_size = actual_read_end - read_start;
    vector<char> buffer(read_size);


    // fiecare proces citeste chunk-ul sau direct din fisier, in paralel
    MPI_File_read_at_all(file, read_start, buffer.data(),
        (int)read_size, MPI_CHAR, MPI_STATUS_IGNORE);
    MPI_File_close(&file);

   
    // CALCULARE START_IDX
    // daca nu suntem primul proces, e posibil ca buffer-ul
    // sa inceapa in mijlocul unui cuvant care apartine procesului anterior
    // il sarim ca sa nu il numaram de doua ori
   
    size_t start_idx = 0;

    if (rank != 0) {
        // sarim literele ramase din cuvantul anterior
        while (start_idx < (size_t)read_size && isalpha((unsigned char)buffer[start_idx]))
            start_idx++;
        // sarim separatorii pana la primul cuvant complet al nostru
        while (start_idx < (size_t)read_size && !isalpha((unsigned char)buffer[start_idx]))
            start_idx++;
        // Acum start_idx pointează la primul cuvânt COMPLET al acestui proces
    }

    // CALCULARE END_IDX
    // daca nu suntem ultimul proces, trebuie sa stabilim unde se termina
    // chunk-ul nostru, avand grija sa nu taiem un cuvant la jumatate
    size_t end_idx = (size_t)read_size; // default: tot buffer-ul (rank == size-1)

    if (rank != size - 1) {
        // granita logica a chunk-ului in interiorul buffer-ului
        size_t boundary = (size_t)(read_end - read_start);

        if (boundary >= (size_t)read_size) {
            // nu am citit extra, procesam tot ce avem
            end_idx = read_size;
        }
        else {
            if (isalpha((unsigned char)buffer[boundary])) {
                // granita cade in mijlocul unui cuvant
                // dam inapoi pana la inceputul lui si il lasam procesului urmator
                size_t pos = boundary;
                while (pos > start_idx && isalpha((unsigned char)buffer[pos - 1]))
                    pos--;
                end_idx = pos;
            }
            else {
                // granita cade pe un separator, chunk-ul nostru e curat
                end_idx = boundary;
            }
        }
    }

    // WORD COUNT LOCAL
    unordered_map<string, int> freq;
    string word;

    for (size_t i = start_idx; i < end_idx; i++) {
        char nc = normalize_char(buffer[i]);
        if (nc != '\0')
            word += nc;     // continuam sa construim cuvantul
        else
            add_word(freq, word);  // am dat de separator, salvam cuvantul
    }
    // salvam ultimul cuvant daca buffer-ul nu se termina cu separator
    add_word(freq, word); 

    // SERIALIZARE
    // convertim map-ul local intr-un string de forma "cuvant count\n"
    // ca sa il putem trimite prin MPI
    string serialized;
    serialized.reserve(freq.size() * 20);
    for (auto& p : freq)
        serialized += p.first + " " + to_string(p.second) + "\n";

    int serialized_size = (int)serialized.size();



    // GATHER DIMENSIUNI
    //colectam dimensiunile de la fiecare proces
    // procesul 0 trebuie sa stie cat spatiu sa aloce pentru fiecare

    vector<int> recvcounts(size);
    MPI_Gather(&serialized_size, 1, MPI_INT,
        recvcounts.data(), 1, MPI_INT,
        0, MPI_COMM_WORLD);


    // GATHER DATE 
    vector<int> displs(size);
    string global;

    if (rank == 0) {
        //calculam offset-ul fiecarui proces in buffer-ul global
        int offset = 0;
        for (int i = 0; i < size; i++) {
            displs[i] = offset;
            offset += recvcounts[i];
        }
        global.resize(offset);
    }

    //fiecare proces trimite rezultatele sale la procesul 0
    MPI_Gatherv(serialized.data(), serialized_size, MPI_CHAR,
        rank == 0 ? &global[0] : nullptr,
        recvcounts.data(), displs.data(), MPI_CHAR,
        0, MPI_COMM_WORLD);

    // REDUCERE FINALA (rank 0)
    //parsam string-ul global si adunam frecventele pentru fiecare cuvant
    if (rank == 0) {
        unordered_map<string, int> global_freq;
        global_freq.reserve(100000);

        size_t i = 0;
        while (i < global.size()) {
            //citim cuvantul pana la spatiu
            size_t word_start = i;
            while (i < global.size() && global[i] != ' ') i++;
            string w(global.begin() + word_start, global.begin() + i);
            i++; // sarim spatiul

            // citim numarul pana la newline
            int count = 0;
            while (i < global.size() && global[i] != '\n')
                count = count * 10 + (global[i++] - '0');
            if (i < global.size()) i++; // skip newline

            // adunam frecventa in map-ul global
            if (!w.empty())
                global_freq[w] += count;
        }

        // sortam descrescator dupa frecventa
        vector<pair<string, int>> v(global_freq.begin(), global_freq.end());
        sort(v.begin(), v.end(),
            [](auto& a, auto& b) { return a.second > b.second; });

        cout << "Cele mai frecvente 10 cuvinte:\n";
        for (int i = 0; i < min(10, (int)v.size()); i++)
            cout << v[i].first << " : " << v[i].second << "\n";
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double end_time = MPI_Wtime();

    if (rank == 0)
        cout << "\nTimp MPI: " << (end_time - start) * 1000 << " ms\n";

    MPI_Finalize();
    return 0;
}