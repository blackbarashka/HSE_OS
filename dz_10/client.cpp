#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 8080

int main() {
    int sockfd;
    struct sockaddr_in serverAddr;
    char buffer[1024];

    // Создание сокета
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Настройка адреса
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Привязка сокета
    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return -1;
    }

    std::cout << "Ожидание сообщений..." << std::endl;

    while (true) {
        socklen_t len = sizeof(serverAddr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&serverAddr, &len);
        if (n < 0) {
            perror("Receive failed");
            break;
        }
        buffer[n] = '\0';
        std::cout << "Получено сообщение: " << buffer << std::endl;
    }

    close(sockfd);
    return 0;
}
