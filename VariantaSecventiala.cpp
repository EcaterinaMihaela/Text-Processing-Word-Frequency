#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <sstream>
#include <cctype>

using namespace std;

//functie pentru normalizare cuvant
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

//fctie de comparare dupa frecventa(elem 2 din pair)
bool cmp(const pair<string, int>& a, const pair<string, int>& b)
{
    return a.second > b.second;
}

int main()
{

    ifstream file("D:/ADP/ProiectAPD/VariantaSecventiala/Alice_in_Wonderland.txt");

    if (!file)
    {
        cout << "Eroare la deschiderea fisierului\n";
        return 1;
    }
    
    unordered_map<string, int> frequency;
    string word;
    //verifica primul caracter fara sa l consume
    if (file.peek() == ifstream::traits_type::eof()) {
        cout << "Fisierul este gol!\n";
        return 1;
    }
    //citire cuvinte
    while (file >> word)
    {
        word = normalize(word);

        if (word != "")
            frequency[word]++;
    }

    //transformare map in vector pentru sortare
    vector<pair<string, int>> words(frequency.begin(), frequency.end());

    //sortare desc vector perechi dupa frecv
    sort(words.begin(), words.end(), cmp);

    int N = 10;

    cout << "Cele mai frecvente " << N << " cuvinte:\n";

    for (int i = 0; i < N && i < words.size(); i++)
    {
        cout << words[i].first << " : " << words[i].second << endl;
    }

    file.close();
    return 0;
}