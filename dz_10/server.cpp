#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BROADCAST_IP "255.255.255.255"

int main() {
    int sockfd;
    struct sockaddr_in broadcastAddr;
    char message[1024];

    // Создание сокета
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Разрешение широковещательной отправки
    int broadcastEnable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("Error in setting broadcast option");
        close(sockfd);
        return -1;
    }

    // Настройка адреса
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(PORT);
    broadcastAddr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    std::cout << "Введите сообщения для отправки (exit для выхода):" << std::endl;

    while (true) {
        std::cin.getline(message, sizeof(message));
        if (strcmp(message, "exit") == 0) {
            break;
        }

        // Отправка сообщения
        if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr)) < 0) {
            perror("Send failed");
        }
    }

    close(sockfd);
    return 0;
}
