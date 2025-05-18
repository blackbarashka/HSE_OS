#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <IP сервера> <PORT сервера>" << std::endl;
        return 1;
    }

    const char* serverIp = argv[1];
    int serverPort = std::stoi(argv[2]);

    // Создание сокета
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Ошибка создания сокета" << std::endl;
        return 1;
    }

    // Настройка адреса сервера
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serverPort);
    
    if (inet_pton(AF_INET, serverIp, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Неверный адрес" << std::endl;
        return 1;
    }

    // Подключение к серверу
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Ошибка подключения" << std::endl;
        return 1;
    }

    std::cout << "=== Наблюдатель за поиском Винни-Пуха ===" << std::endl;
    std::cout << "Подключение к серверу: " << serverIp << ":" << serverPort << std::endl;

    // Отправка идентификатора OBSERVER
    std::string message = "OBSERVER";
    send(sock, message.c_str(), message.size(), 0);

    // Получение и отображение информации от сервера
    char buffer[1024] = {0};
    while (true) {
        int valread = read(sock, buffer, sizeof(buffer) - 1);
        if (valread <= 0) {
            std::cout << "Соединение с сервером разорвано." << std::endl;
            break;
        }
        buffer[valread] = '\0';
        std::cout << buffer << std::endl;
    }

    close(sock);
    return 0;
}
