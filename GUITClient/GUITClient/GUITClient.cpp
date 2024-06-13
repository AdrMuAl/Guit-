#include <iostream>
#include <WS2tcpip.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#pragma comment (lib, "WS2_32.lib")

using namespace std;

// Función para enviar un comando al servidor y recibir la respuesta
void sendCommand(SOCKET& sock, const string& command) {
    int sendResult = send(sock, command.c_str(), command.size() + 1, 0);
    if (sendResult == SOCKET_ERROR) {
        cerr << "Failed to send command to server" << endl;
        closesocket(sock);
        WSACleanup();
        exit(1);
    }

    // Recibir respuesta
    char buf[4096];
    ZeroMemory(buf, 4096);
    int bytesReceived = recv(sock, buf, 4096, 0);
    if (bytesReceived > 0) {
        cout << "Server: " << string(buf, 0, bytesReceived) << endl;
    }
}

// Función para el comando 'guit init <name>'
// Inicializa un nuevo repositorio en el servidor
void initCommand(SOCKET& sock, const string& name) {
    string command = "guit init " + name;
    sendCommand(sock, command);
}

// Función para el comando 'guit help'
// Muestra la lista de comandos disponibles
void helpCommand(SOCKET& sock) {
    string command = "guit help";
    sendCommand(sock, command);
}

// Función para el comando 'guit add <repo> <file> <content>'
// Agrega un archivo específico al repositorio
void addCommand(SOCKET& sock, const vector<string>& params) {
    if (params.size() < 2) {
        cerr << "Invalid add command usage. Expected: guit add <repo> <file>" << endl;
        return;
    }

    string repoPath = params[0];
    string fileName = params[1];

    ifstream file(fileName, ios::binary);
    if (!file.is_open()) {
        cerr << "Failed to open file: " << fileName << endl;
        return;
    }

    // Leer el contenido del archivo
    vector<char> fileContents((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    string fileContentStr(fileContents.begin(), fileContents.end());
    file.close();

    string command = "guit add " + repoPath + " " + fileName + " " + fileContentStr;
    sendCommand(sock, command);
}

// Función para el comando 'guit commit <mensaje>'
// Realiza un commit con un mensaje descriptivo
void commitCommand(SOCKET& sock, const string& message) {
    string command = "guit commit " + message;
    sendCommand(sock, command);
}

// Función para el comando 'guit status <file>'
// Muestra el estado de un archivo específico
void statusCommand(SOCKET& sock, const string& file) {
    string command = "guit status " + file;
    sendCommand(sock, command);
}

// Función para el comando 'guit rollback <file> <commit>'
// Reverte un archivo a una versión específica indicada por el ID de commit
void rollbackCommand(SOCKET& sock, const string& file, const string& commitId) {
    string command = "guit rollback " + file + " " + commitId;
    sendCommand(sock, command);
}

// Función para el comando 'guit reset <file>'
// Restaura un archivo al último commit
void resetCommand(SOCKET& sock, const string& file) {
    string command = "guit reset " + file;
    sendCommand(sock, command);
}

// Función para el comando 'guit sync <file>'
// Sincroniza un archivo con el servidor
void syncCommand(SOCKET& sock, const string& file) {
    string command = "guit sync " + file;
    sendCommand(sock, command);
}

// Función para el comando 'guit query <sql>'
// Permite al cliente enviar consultas SQL directamente al servidor y obtener resultados
void queryCommand(SOCKET& sock, const string& sql) {
    string command = "guit query " + sql;
    sendCommand(sock, command);
}

// Función para dividir una cadena de texto en tokens usando un delimitador específico
vector<string> split(const string& s, char delimiter) {
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

int main() {
    // Inicializar Winsock
    WSADATA data;
    WORD ver = MAKEWORD(2, 2);
    int wsResult = WSAStartup(ver, &data);
    if (wsResult != 0) {
        cerr << "Can't start Winsock, Err #" << wsResult << endl;
        return 1;
    }

    // Crear socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cerr << "Can't create socket, Err #" << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    // Rellenar una estructura de hint
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(54000);
    inet_pton(AF_INET, "127.0.0.1", &hint.sin_addr);

    // Conectar al servidor
    int connResult = connect(sock, (sockaddr*)&hint, sizeof(hint));
    if (connResult == SOCKET_ERROR) {
        cerr << "Can't connect to server, Err #" << WSAGetLastError() << endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    // Bucle principal
    string userInput;
    do {
        cout << "> ";
        getline(cin, userInput);

        vector<string> tokens = split(userInput, ' ');
        if (tokens.empty()) continue;

        string command = tokens[0];

        if (command == "guit") {
            if (tokens.size() > 1) {
                string subCommand = tokens[1];

                if (subCommand == "init" && tokens.size() == 3) {
                    initCommand(sock, tokens[2]);
                }
                else if (subCommand == "help") {
                    helpCommand(sock);
                }
                else if (subCommand == "add") {
                    vector<string> params(tokens.begin() + 2, tokens.end());
                    addCommand(sock, params);
                }
                else if (subCommand == "commit" && tokens.size() >= 3) {
                    commitCommand(sock, userInput.substr(11));
                }
                else if (subCommand == "status" && tokens.size() == 3) {
                    statusCommand(sock, tokens[2]);
                }
                else if (subCommand == "rollback" && tokens.size() == 4) {
                    rollbackCommand(sock, tokens[2], tokens[3]);
                }
                else if (subCommand == "reset" && tokens.size() == 3) {
                    resetCommand(sock, tokens[2]);
                }
                else if (subCommand == "sync" && tokens.size() == 3) {
                    syncCommand(sock, tokens[2]);
                }
                else if (subCommand == "query" && tokens.size() >= 3) {
                    string sqlQuery = userInput.substr(11);
                    queryCommand(sock, sqlQuery);
                }
                else {
                    cout << "Invalid command or parameters. Type 'guit help' for a list of commands." << endl;
                }
            }
            else {
                cout << "Invalid command or parameters. Type 'guit help' for a list of commands." << endl;
            }
        }
        else {
            cout << "Commands should start with 'guit'. Type 'guit help' for a list of commands." << endl;
        }

    } while (userInput.size() > 0);

    // Cerrar todo correctamente
    closesocket(sock);
    WSACleanup();
    return 0;
}
