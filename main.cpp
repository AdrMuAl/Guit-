#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>

void compareFiles(const std::string& file1, const std::string& file2) {
    std::ifstream fileStream1(file1);
    std::ifstream fileStream2(file2);

    if (!fileStream1.is_open()) {
        std::cerr << "No se pudo abrir el archivo " << file1 << std::endl;
        return;
    }

    if (!fileStream2.is_open()) {
        std::cerr << "No se pudo abrir el archivo " << file2 << std::endl;
        return;
    }

    std::unordered_set<std::string> linesInFile2;
    std::string line2;
    int lineNum = 1;
    while (std::getline(fileStream2, line2)) {
        linesInFile2.insert(line2);
        lineNum++;
    }

    std::string line1;
    lineNum = 1;
    while (std::getline(fileStream1, line1)) {
        if (linesInFile2.find(line1) == linesInFile2.end()) {
            std::cout << lineNum << line1 << std::endl;
        }
        lineNum++;
    }

    fileStream1.close();
    fileStream2.close();
}

int main() {
    //Imprime lo que file1 tiene que file2 no tiene.
    std::string file2 = "C:\\Users\\chris\\Desktop\\Pruebaa\\texto.txt";
    std::string file1 = "C:\\Users\\chris\\Desktop\\Pruebaa\\texto_2.txt";

    compareFiles(file1, file2);

    return 0;
}