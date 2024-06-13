#include <iostream>
#include <WS2tcpip.h>
#include <sqlite3.h>
#include <string>
#include <sstream>
#include <vector>
#include <fstream>
#include <direct.h> // Para _mkdir

#pragma comment (lib, "WS2_32.lib")

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
        // Create directory for the new repository
        if (_mkdir(name.c_str()) == 0) {
            // Create .guitignore file in the new repository
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

void handleAddCommand(sqlite3* DB, const vector<string>& params, SOCKET clientSocket) {
    // Implementation of add command
    string response = "Add command received";
    send(clientSocket, response.c_str(), response.size() + 1, 0);
}

void handleCommitCommand(sqlite3* DB, const string& message, SOCKET clientSocket) {
    // Implementation of commit command
    string response = "Commit command received";
    send(clientSocket, response.c_str(), response.size() + 1, 0);
}

void handleStatusCommand(sqlite3* DB, const string& file, SOCKET clientSocket) {
    // Implementation of status command
    string response = "Status command received";
    send(clientSocket, response.c_str(), response.size() + 1, 0);
}

void handleRollbackCommand(sqlite3* DB, const string& file, const string& commitId, SOCKET clientSocket) {
    // Implementation of rollback command
    string response = "Rollback command received";
    send(clientSocket, response.c_str(), response.size() + 1, 0);
}

void handleResetCommand(sqlite3* DB, const string& file, SOCKET clientSocket) {
    // Implementation of reset command
    string response = "Reset command received";
    send(clientSocket, response.c_str(), response.size() + 1, 0);
}

void handleSyncCommand(sqlite3* DB, const string& file, SOCKET clientSocket) {
    // Implementation of sync command
    string response = "Sync command received";
    send(clientSocket, response.c_str(), response.size() + 1, 0);
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
            result += string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, i))) + " | ";
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
            else if (subCommand == "add") {
                vector<string> params(tokens.begin() + 2, tokens.end());
                handleAddCommand(DB, params, clientSocket);
            }
            else if (subCommand == "commit" && tokens.size() >= 3) {
                handleCommitCommand(DB, message.substr(11), clientSocket);
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

    // initialize winsock
    WSADATA WSData;
    WORD ver = MAKEWORD(2, 2);

    int wsOk = WSAStartup(ver, &WSData);
    if (wsOk != 0) {
        cerr << "Can't initialize winsock" << endl;
        return 1;
    }

    // create a socket
    SOCKET listening = socket(AF_INET, SOCK_STREAM, 0);
    if (listening == INVALID_SOCKET) {
        cerr << "Can't create socket" << endl;
        return 1;
    }

    // bind the ip address and port to a socket
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(54000);
    hint.sin_addr.S_un.S_addr = INADDR_ANY;

    bind(listening, (sockaddr*)&hint, sizeof(hint));

    // tell winsock is ready for listening
    listen(listening, SOMAXCONN);

    // wait for connection
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

    // close listening socket
    closesocket(listening);

    // while loop: accept and process message from client
    char buf[4096];
    while (true) {
        ZeroMemory(buf, 4096);

        // wait for client data
        int bytesReceived = recv(clientSocket, buf, 4096, 0);
        if (bytesReceived == SOCKET_ERROR) {
            cerr << "Error in recv()" << endl;
            break;
        }
        if (bytesReceived == 0) {
            cout << "Client disconnected" << endl;
            break;
        }

        // Process message
        string clientMessage(buf, 0, bytesReceived);
        processClientMessage(clientSocket, DB, clientMessage);
    }

    // close socket
    closesocket(clientSocket);

    // shutdown winsock
    WSACleanup();
    sqlite3_close(DB);

    return 0;
}
