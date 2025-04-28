#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char* argv[])
{
    std::string serverIP = "127.0.0.1";
    int port = 6000;
    bool isSender = false; // Клиент №1 "send", Клиент №2 "recv"

    if (argc > 1) {
        serverIP = argv[1];
    }
    if (argc > 2) {
        port = std::atoi(argv[2]);
    }
    if (argc > 3) {
        std::string mode = argv[3];
        if (mode == "send") {
            isSender = true;
        }
    }

    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        std::cerr << "Ошибка при создании сокета\n";
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Неверный IP адрес\n";
        close(clientSocket);
        return 1;
    }

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Ошибка connect\n";
        close(clientSocket);
        return 1;
    }
    std::cout << "Подключено к " << serverIP << ":" << port << "\n";

    if (isSender) {
        // Клиент №1: отправка
        while (true) {
            std::cout << "> ";
            std::string msg;
            std::getline(std::cin, msg);

            if (send(clientSocket, msg.c_str(), msg.size(), 0) < 0) {
                std::cerr << "Ошибка send\n";
                break;
            }

            if (msg == "The End") {
                break;
            }
        }
    } else {
        // Клиент №2: прием
        char buffer[1024];
        while (true) {
            std::memset(buffer, 0, sizeof(buffer));
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesRead <= 0) {
                std::cerr << "Сервер отключился\n";
                break;
            }
            std::string msg(buffer);
            std::cout << "Получено: " << msg << "\n";

            if (msg == "The End") {
                break;
            }
        }
    }

    close(clientSocket);
    return 0;
}
