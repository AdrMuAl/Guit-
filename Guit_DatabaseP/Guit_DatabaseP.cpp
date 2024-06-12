// Guit_DatabaseP.cpp : Este archivo contiene la función 'main'. La ejecución del programa comienza y termina ahí.
//

#include <iostream>
#include "sqlite/sqlite3.h"
using namespace std;

int main() {
    sqlite3* db;
    char* zErrMsg = 0;
    int rc;

    // Abrir la base de datos
    rc = sqlite3_open("Guit_Database.db", &db);
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }
    else {
        std::cout << "Opened database successfully" << std::endl;
    }

    // Crear la tabla Estudiante si no existe
    const char* createTableSQL1 = "CREATE TABLE IF NOT EXISTS Estudiante (ID INTEGER PRIMARY KEY, Nota TEXT);";
    rc = sqlite3_exec(db, createTableSQL1, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    }
    else {
        std::cout << "Table 'Estudiante' created successfully" << std::endl;
    }

    // Insertar letras en la tabla Estudiante
    const char* insertSQL1 = "INSERT INTO Estudiante (Nota) VALUES (?);";
    sqlite3_stmt* stmt1;
    rc = sqlite3_prepare_v2(db, insertSQL1, -1, &stmt1, 0);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return 1;
    }

    const char* letras1[5] = { "A", "B", "C", "D", "E" };

    for (int i = 0; i < 5; ++i) {
        sqlite3_bind_text(stmt1, 1, letras1[i], -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt1);
        if (rc != SQLITE_DONE) {
            std::cerr << "Execution failed: " << sqlite3_errmsg(db) << std::endl;
        }
        else {
            std::cout << "Inserted note " << letras1[i] << " into 'Estudiante' successfully" << std::endl;
        }
        sqlite3_reset(stmt1);
    }
    sqlite3_finalize(stmt1);

    // Crear la tabla Estudiante2 si no existe
    const char* createTableSQL2 = "CREATE TABLE IF NOT EXISTS Estudiante2 (ID INTEGER PRIMARY KEY, Nota TEXT);";
    rc = sqlite3_exec(db, createTableSQL2, 0, 0, &zErrMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << zErrMsg << std::endl;
        sqlite3_free(zErrMsg);
    }
    else {
        std::cout << "Table 'Estudiante2' created successfully" << std::endl;
    }

    // Insertar letras en la tabla Estudiante2
    const char* insertSQL2 = "INSERT INTO Estudiante2 (Nota) VALUES (?);";
    sqlite3_stmt* stmt2;
    rc = sqlite3_prepare_v2(db, insertSQL2, -1, &stmt2, 0);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        return 1;
    }

    const char* letras2[5] = { "F", "G", "H", "I", "J" };

    for (int i = 0; i < 5; ++i) {
        sqlite3_bind_text(stmt2, 1, letras2[i], -1, SQLITE_STATIC);
        rc = sqlite3_step(stmt2);
        if (rc != SQLITE_DONE) {
            std::cerr << "Execution failed: " << sqlite3_errmsg(db) << std::endl;
        }
        else {
            std::cout << "Inserted note " << letras2[i] << " into 'Estudiante2' successfully" << std::endl;
        }
        sqlite3_reset(stmt2);
    }
    sqlite3_finalize(stmt2);

    // Cerrar la base de datos
    sqlite3_close(db);
    std::cout << "Database closed" << std::endl;

    return 0;
}



// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
