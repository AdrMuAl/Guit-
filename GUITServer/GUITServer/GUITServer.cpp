#include <iostream>
#include <WS2tcpip.h>
#include <sqlite3.h>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <direct.h>
#include <windows.h>
#include <wincrypt.h>

#pragma comment (lib, "WS2_32.lib")
#pragma comment (lib, "Crypt32.lib")

using namespace std;

void createDatabase(const char* databaseName) {
    sqlite3* DB;
    int exit = sqlite3_open(databaseName, &DB);

    if (exit) {
        cerr << "Error opening database: " << sqlite3_errmsg(DB) << endl;
        return;
    }
    else {
        cout << "Database created successfully!" << endl;
    }

    const char* sql = R"(
    CREATE TABLE IF NOT EXISTS Repositorios (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        nombre TEXT NOT NULL
    );
    CREATE TABLE IF NOT EXISTS Archivos (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        repositorio_id INTEGER,
        nombre TEXT NOT NULL,
        contenido BLOB,
        pendiente_commit INTEGER DEFAULT 1,
        FOREIGN KEY (repositorio_id) REFERENCES Repositorios (id)
    );
    CREATE TABLE IF NOT EXISTS Commits (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        repositorio_id INTEGER,
        mensaje TEXT NOT NULL,
        fecha DATETIME DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (repositorio_id) REFERENCES Repositorios (id)
    );
    CREATE TABLE IF NOT EXISTS Versiones (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        archivo_id INTEGER,
        commit_id INTEGER,
        contenido BLOB,
        checksum TEXT,
        fecha DATETIME DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY (archivo_id) REFERENCES Archivos (id),
        FOREIGN KEY (commit_id) REFERENCES Commits (id)
    );
)";

    char* errorMessage;
    exit = sqlite3_exec(DB, sql, 0, 0, &errorMessage);

    if (exit != SQLITE_OK) {
        cerr << "Error creating tables: " << errorMessage << endl;
        sqlite3_free(errorMessage);
    }
    else {
        cout << "Tables created successfully!" << endl;
    }

    sqlite3_close(DB);
}

void handleInitCommand(sqlite3* DB, const string& name, SOCKET clientSocket) {
    string sql = "INSERT INTO Repositorios (nombre) VALUES ('" + name + "');";
    char* errorMessage;
    int exit = sqlite3_exec(DB, sql.c_str(), 0, 0, &errorMessage);

    if (exit != SQLITE_OK) {
        cerr << "Error executing init command: " << errorMessage << endl;
        sqlite3_free(errorMessage);
        send(clientSocket, "Error creating repository", 25, 0);
    }
    else {
        // Crear directorio para el nuevo repositorio
        if (_mkdir(name.c_str()) == 0) {
            // Crear archivo .guitignore en el nuevo repositorio
            ofstream ignoreFile(name + "/.guitignore");
            if (ignoreFile.is_open()) {
                ignoreFile.close();
                send(clientSocket, "Repository created successfully", 30, 0);
            }
            else {
                send(clientSocket, "Error creating .guitignore file", 31, 0);
            }
        }
        else {
            send(clientSocket, "Error creating repository directory", 35, 0);
        }
    }
}

void handleHelpCommand(SOCKET clientSocket) {
    string helpMessage =
        "Available commands:\n"
        "guit init <name>\n"
        "guit help\n"
        "guit add [-A] [name]\n"
        "guit commit <mensaje>\n"
        "guit status <file>\n"
        "guit rollback <file> <commit>\n"
        "guit reset <file>\n"
        "guit sync <file>\n"
        "guit query <sql>";
    send(clientSocket, helpMessage.c_str(), helpMessage.size() + 1, 0);
}

string calculateMD5(const vector<char>& data) {
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE rgbHash[16];
    DWORD cbHash = 16;
    CHAR rgbDigits[] = "0123456789abcdef";
    string md5String;

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
            if (CryptHashData(hHash, reinterpret_cast<const BYTE*>(data.data()), data.size(), 0)) {
                if (CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0)) {
                    for (DWORD i = 0; i < cbHash; i++) {
                        md5String.append(&rgbDigits[rgbHash[i] >> 4], 1);
                        md5String.append(&rgbDigits[rgbHash[i] & 0xf], 1);
                    }
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    return md5String;
}

void handleCommitCommand(sqlite3* DB, const vector<string>& params, const string& message, SOCKET clientSocket) {
    if (params.size() < 2) {
        send(clientSocket, "No files specified for commit", 29, 0);
        return;
    }

    // Insertar commit en la base de datos
    string insertCommitSql = "INSERT INTO Commits (repositorio_id, mensaje) VALUES ((SELECT id FROM Repositorios ORDER BY id DESC LIMIT 1), ?);";
    sqlite3_stmt* commitStmt;
    int rc = sqlite3_prepare_v2(DB, insertCommitSql.c_str(), -1, &commitStmt, NULL);
    if (rc != SQLITE_OK) {
        cerr << "Error preparing statement: " << sqlite3_errmsg(DB) << endl;
        send(clientSocket, "Error preparing statement", 26, 0);
        return;
    }

    sqlite3_bind_text(commitStmt, 1, message.c_str(), -1, SQLITE_STATIC);

    rc = sqlite3_step(commitStmt);
    if (rc != SQLITE_DONE) {
        cerr << "Error inserting commit: " << sqlite3_errmsg(DB) << endl;
        send(clientSocket, "Error inserting commit", 22, 0);
        sqlite3_finalize(commitStmt);
        return;
    }
    sqlite3_finalize(commitStmt);

    // Obtener el ID del commit recién insertado
    int commitId = sqlite3_last_insert_rowid(DB);

    // Procesar cada archivo
    for (size_t i = 1; i < params.size(); i += 2) {
        string fileName = params[i];
        string fileContentStr = params[i + 1];
        vector<char> fileContent(fileContentStr.begin(), fileContentStr.end());

        // Calcular el checksum del archivo
        string checksum = calculateMD5(fileContent);

        // Insertar archivo en la base de datos si no existe
        string insertFileSql = "INSERT INTO Archivos (repositorio_id, nombre, contenido, pendiente_commit) VALUES ((SELECT id FROM Repositorios ORDER BY id DESC LIMIT 1), ?, ?, 0) ON CONFLICT(nombre) DO UPDATE SET contenido=excluded.contenido, pendiente_commit=0;";
        sqlite3_stmt* fileStmt;
        int rc = sqlite3_prepare_v2(DB, insertFileSql.c_str(), -1, &fileStmt, NULL);
        if (rc != SQLITE_OK) {
            cerr << "Error preparing statement: " << sqlite3_errmsg(DB) << endl;
            continue;
        }

        sqlite3_bind_text(fileStmt, 1, fileName.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_blob(fileStmt, 2, fileContent.data(), fileContent.size(), SQLITE_TRANSIENT);

        rc = sqlite3_step(fileStmt);
        if (rc != SQLITE_DONE) {
            cerr << "Error inserting file: " << sqlite3_errmsg(DB) << endl;
        }
        sqlite3_finalize(fileStmt);

        // Obtener el ID del archivo
        int fileId = sqlite3_last_insert_rowid(DB);

        // Insertar versión del archivo
        string insertVersionSql = "INSERT INTO Versiones (archivo_id, commit_id, contenido, checksum) VALUES (?, ?, ?, ?);";
        sqlite3_stmt* versionStmt;
        rc = sqlite3_prepare_v2(DB, insertVersionSql.c_str(), -1, &versionStmt, NULL);
        if (rc != SQLITE_OK) {
            cerr << "Error preparing statement: " << sqlite3_errmsg(DB) << endl;
            continue;
        }

        sqlite3_bind_int(versionStmt, 1, fileId);
        sqlite3_bind_int(versionStmt, 2, commitId);
        sqlite3_bind_blob(versionStmt, 3, fileContent.data(), fileContent.size(), SQLITE_TRANSIENT);
        sqlite3_bind_text(versionStmt, 4, checksum.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(versionStmt);
        if (rc != SQLITE_DONE) {
            cerr << "Error inserting version: " << sqlite3_errmsg(DB) << endl;
        }
        sqlite3_finalize(versionStmt);
    }

    send(clientSocket, "Commit created successfully", 27, 0);
}

void handleStatusCommand(sqlite3* DB, const string& file, SOCKET clientSocket) {
    if (file == "-A") {
        string sql = "SELECT A.nombre, A.pendiente_commit FROM Archivos A WHERE A.pendiente_commit = 1;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(DB, sql.c_str(), -1, &stmt, NULL);

        if (rc != SQLITE_OK) {
            string errorMessage = "Failed to execute status command: " + string(sqlite3_errmsg(DB));
            send(clientSocket, errorMessage.c_str(), errorMessage.size() + 1, 0);
            return;
        }

        string response;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            string fileName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            int pendingCommit = sqlite3_column_int(stmt, 1);
            response += "File: " + fileName + " - " + (pendingCommit ? "Pending Commit" : "Committed") + "\n";
        }

        send(clientSocket, response.c_str(), response.size() + 1, 0);
        sqlite3_finalize(stmt);
    }
    else {
        string sql = "SELECT V.checksum, V.fecha, C.mensaje FROM Versiones V INNER JOIN Archivos A ON V.archivo_id = A.id INNER JOIN Commits C ON V.commit_id = C.id WHERE A.nombre = ? ORDER BY V.fecha DESC;";
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(DB, sql.c_str(), -1, &stmt, NULL);

        if (rc != SQLITE_OK) {
            string errorMessage = "Failed to execute status command: " + string(sqlite3_errmsg(DB));
            send(clientSocket, errorMessage.c_str(), errorMessage.size() + 1, 0);
            return;
        }

        sqlite3_bind_text(stmt, 1, file.c_str(), -1, SQLITE_STATIC);

        string response;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            response += "Checksum: " + string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0))) + "\n";
            response += "Fecha: " + string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))) + "\n";
            response += "Mensaje: " + string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2))) + "\n\n";
        }

        if (response.empty()) {
            response = "No versions found for file: " + file;
        }

        send(clientSocket, response.c_str(), response.size() + 1, 0);
        sqlite3_finalize(stmt);
    }
}

void handleRollbackCommand(sqlite3* DB, const string& file, const string& commitId, SOCKET clientSocket) {
    string sql = "SELECT V.contenido FROM Versiones V INNER JOIN Archivos A ON V.archivo_id = A.id WHERE A.nombre = ? AND V.commit_id = ?;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(DB, sql.c_str(), -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        string errorMessage = "Failed to execute rollback command: " + string(sqlite3_errmsg(DB));
        send(clientSocket, errorMessage.c_str(), errorMessage.size() + 1, 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, file.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, commitId.c_str(), -1, SQLITE_STATIC);

    string response;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void* blob = sqlite3_column_blob(stmt, 0);
        int blobSize = sqlite3_column_bytes(stmt, 0);

        ofstream outFile(file, ios::binary);
        outFile.write(reinterpret_cast<const char*>(blob), blobSize);
        outFile.close();

        response = "File rolled back to commit " + commitId;
    }
    else {
        response = "File or commit not found";
    }

    send(clientSocket, response.c_str(), response.size() + 1, 0);
    sqlite3_finalize(stmt);
}

void handleResetCommand(sqlite3* DB, const string& file, SOCKET clientSocket) {
    string sql = "SELECT contenido FROM Versiones V INNER JOIN Archivos A ON V.archivo_id = A.id WHERE A.nombre = ? ORDER BY V.fecha DESC LIMIT 1;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(DB, sql.c_str(), -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        string errorMessage = "Failed to execute reset command: " + string(sqlite3_errmsg(DB));
        send(clientSocket, errorMessage.c_str(), errorMessage.size() + 1, 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, file.c_str(), -1, SQLITE_STATIC);

    string response;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ofstream outFile(file, ios::binary);
        outFile.write(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 0)), sqlite3_column_bytes(stmt, 0));
        outFile.close();
        response = "File reset to the latest version";
    }
    else {
        response = "File not found or no versions available";
    }

    send(clientSocket, response.c_str(), response.size() + 1, 0);
    sqlite3_finalize(stmt);
}

void handleSyncCommand(sqlite3* DB, const string& file, SOCKET clientSocket) {
    string sql = "SELECT contenido FROM Versiones V INNER JOIN Archivos A ON V.archivo_id = A.id WHERE A.nombre = ? ORDER BY V.fecha DESC LIMIT 1;";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(DB, sql.c_str(), -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        string errorMessage = "Failed to execute sync command: " + string(sqlite3_errmsg(DB));
        send(clientSocket, errorMessage.c_str(), errorMessage.size() + 1, 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, file.c_str(), -1, SQLITE_STATIC);

    string response;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ofstream outFile(file, ios::binary);
        outFile.write(reinterpret_cast<const char*>(sqlite3_column_blob(stmt, 0)), sqlite3_column_bytes(stmt, 0));
        outFile.close();
        response = "File synchronized successfully";
    }
    else {
        response = "File not found or no versions available";
    }

    send(clientSocket, response.c_str(), response.size() + 1, 0);
    sqlite3_finalize(stmt);
}

void handleQueryCommand(sqlite3* DB, const string& sqlQuery, SOCKET clientSocket) {
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(DB, sqlQuery.c_str(), -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        string errorMessage = "Failed to execute query: " + string(sqlite3_errmsg(DB));
        send(clientSocket, errorMessage.c_str(), errorMessage.size() + 1, 0);
        return;
    }

    string result;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int columns = sqlite3_column_count(stmt);
        for (int i = 0; i < columns; i++) {
            const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            result += (text ? text : "NULL") + string(" | ");
        }
        result += "\n";
    }

    if (result.empty()) {
        result = "No results found";
    }

    send(clientSocket, result.c_str(), result.size() + 1, 0);
    sqlite3_finalize(stmt);
}

vector<string> split(const string& s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void processClientMessage(SOCKET clientSocket, sqlite3* DB, const string& message) {
    vector<string> tokens = split(message, ' ');
    if (tokens.empty()) return;

    string command = tokens[0];

    if (command == "guit") {
        if (tokens.size() > 1) {
            string subCommand = tokens[1];

            if (subCommand == "init" && tokens.size() == 3) {
                handleInitCommand(DB, tokens[2], clientSocket);
            }
            else if (subCommand == "help") {
                handleHelpCommand(clientSocket);
            }
            else if (subCommand == "commit" && tokens.size() >= 3) {
                vector<string> params(tokens.begin() + 2, tokens.end());
                handleCommitCommand(DB, params, message.substr(11), clientSocket);
            }
            else if (subCommand == "status" && tokens.size() == 3) {
                handleStatusCommand(DB, tokens[2], clientSocket);
            }
            else if (subCommand == "rollback" && tokens.size() == 4) {
                handleRollbackCommand(DB, tokens[2], tokens[3], clientSocket);
            }
            else if (subCommand == "reset" && tokens.size() == 3) {
                handleResetCommand(DB, tokens[2], clientSocket);
            }
            else if (subCommand == "sync" && tokens.size() == 3) {
                handleSyncCommand(DB, tokens[2], clientSocket);
            }
            else if (subCommand == "query" && tokens.size() >= 3) {
                string sqlQuery = message.substr(11);
                handleQueryCommand(DB, sqlQuery, clientSocket);
            }
            else {
                send(clientSocket, "Invalid command or parameters", 29, 0);
            }
        }
        else {
            send(clientSocket, "Invalid command or parameters", 29, 0);
        }
    }
    else {
        send(clientSocket, "Commands should start with 'guit'", 34, 0);
    }
}

int main() {
    createDatabase("guit.db");

    sqlite3* DB;
    int exit = sqlite3_open("guit.db", &DB);
    if (exit) {
        cerr << "Error opening database: " << sqlite3_errmsg(DB) << endl;
        return 1;
    }

    // Inicializar Winsock
    WSADATA WSData;
    WORD ver = MAKEWORD(2, 2);

    int wsOk = WSAStartup(ver, &WSData);
    if (wsOk != 0) {
        cerr << "Can't initialize winsock" << endl;
        return 1;
    }

    // Crear un socket
    SOCKET listening = socket(AF_INET, SOCK_STREAM, 0);
    if (listening == INVALID_SOCKET) {
        cerr << "Can't create socket" << endl;
        return 1;
    }

    // Asignar la dirección IP y el puerto al socket
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(54000);
    hint.sin_addr.S_un.S_addr = INADDR_ANY;

    bind(listening, (sockaddr*)&hint, sizeof(hint));

    // Decirle a Winsock que el socket está listo para escuchar
    listen(listening, SOMAXCONN);

    // Esperar a una conexión
    sockaddr_in client;
    int clientSize = sizeof(client);

    SOCKET clientSocket = accept(listening, (sockaddr*)&client, &clientSize);

    char host[NI_MAXHOST];
    char service[NI_MAXHOST];

    ZeroMemory(host, NI_MAXHOST);
    ZeroMemory(service, NI_MAXHOST);

    if (getnameinfo((sockaddr*)&client, sizeof(client), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0) {
        cout << host << " connected on port " << service << endl;
    }
    else {
        inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
        cout << host << " connected on port " << ntohs(client.sin_port) << endl;
    }

    // Cerrar el socket de escucha
    closesocket(listening);

    // Bucle: aceptar y procesar mensajes del cliente
    char buf[4096];
    while (true) {
        ZeroMemory(buf, 4096);

        // Esperar datos del cliente
        int bytesReceived = recv(clientSocket, buf, 4096, 0);
        if (bytesReceived == SOCKET_ERROR) {
            cerr << "Error in recv()" << endl;
            break;
        }
        if (bytesReceived == 0) {
            cout << "Client disconnected" << endl;
            break;
        }

        // Procesar mensaje
        string clientMessage(buf, 0, bytesReceived);
        processClientMessage(clientSocket, DB, clientMessage);
    }

    // Cerrar el socket
    closesocket(clientSocket);

    // Apagar Winsock
    WSACleanup();
    sqlite3_close(DB);

    return 0;
}
