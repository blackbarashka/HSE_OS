#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define MULTICAST_IP "239.255.255.250" // Многоадресный IP-адрес

int main() {
    int sockfd;
    struct sockaddr_in multicastAddr;
    char message[1024];

    // Создание сокета
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Настройка адреса
    memset(&multicastAddr, 0, sizeof(multicastAddr));
    multicastAddr.sin_family = AF_INET;
    multicastAddr.sin_port = htons(PORT);
    multicastAddr.sin_addr.s_addr = inet_addr(MULTICAST_IP);

    std::cout << "Введите сообщения для отправки (exit для выхода):" << std::endl;

    while (true) {
        std::cin.getline(message, sizeof(message));
        if (strcmp(message, "exit") == 0) {
            break;
        }

        // Отправка сообщения
        if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&multicastAddr, sizeof(multicastAddr)) < 0) {
            perror("Send failed");
        }
    }

    close(sockfd);
    return 0;
}
