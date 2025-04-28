#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char* argv[])
{
    int port = 6000;
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Ошибка при создании сокета\n";
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Ошибка bind\n";
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, 2) < 0) {
        std::cerr << "Ошибка listen\n";
        close(serverSocket);
        return 1;
    }

    std::cout << "Ожидание подключений на порту " << port << "...\n";

    // Принять подключение клиентa №1
    sockaddr_in clientAddr1{};
    socklen_t clientLen1 = sizeof(clientAddr1);
    int client1 = accept(serverSocket, (sockaddr*)&clientAddr1, &clientLen1);
    if (client1 < 0) {
        std::cerr << "Ошибка accept (клиент №1)\n";
        close(serverSocket);
        return 1;
    }
    std::cout << "Клиент №1 подключен.\n";

    // Принять подключение клиента №2
    sockaddr_in clientAddr2{};
    socklen_t clientLen2 = sizeof(clientAddr2);
    int client2 = accept(serverSocket, (sockaddr*)&clientAddr2, &clientLen2);
    if (client2 < 0) {
        std::cerr << "Ошибка accept (клиент №2)\n";
        close(client1);
        close(serverSocket);
        return 1;
    }
    std::cout << "Клиент №2 подключен.\n";

    char buffer[1024];
    while (true) {
        std::memset(buffer, 0, sizeof(buffer));
        int bytesRead = recv(client1, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            std::cerr << "Клиент №1 отключился.\n";
            break;
        }

        std::string message(buffer);
        std::cout << "Получено сообщение от клиента №1: " << message << "\n";

        // Пересылка клиенту №2
        if (send(client2, message.c_str(), message.size(), 0) < 0) {
            std::cerr << "Ошибка отправки клиенту №2\n";
            break;
        }

        if (message == "The End") {
            std::cout << "Завершение...\n";
            break;
        }
    }

    close(client1);
    close(client2);
    close(serverSocket);
    return 0;
}
