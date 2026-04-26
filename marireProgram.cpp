#include <fstream>
#include <string>
using namespace std;

int main() {
    ifstream in("D:/ADP/ProiectAPD/VariantaSecventiala/Alice_in_Wonderland_AllChapters.txt");
    ofstream out("D:/ADP/ProiectAPD/VariantaSecventiala/Alice_in_Wonderland_x30mb.txt");


    string line;

    for (int i = 0; i < 283; i++) 
    {
        in.clear();
        in.seekg(0);

        while (getline(in, line))
        {
            out << line << "\n";
        }
    }
}