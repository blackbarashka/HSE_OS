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
    struct sockaddr_in localAddr;
    struct ip_mreq multicastRequest;
    char buffer[1024];

    // Создание сокета
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Настройка локального адреса
    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(PORT);
    localAddr.sin_addr.s_addr = INADDR_ANY;

    // Привязка сокета
    if (bind(sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return -1;
    }

    // Присоединение к многоадресной группе
    multicastRequest.imr_multiaddr.s_addr = inet_addr(MULTICAST_IP);
    multicastRequest.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &multicastRequest, sizeof(multicastRequest)) < 0) {
        perror("Failed to join multicast group");
        close(sockfd);
        return -1;
    }

    std::cout << "Ожидание сообщений..." << std::endl;

    while (true) {
        socklen_t len = sizeof(localAddr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&localAddr, &len);
        if (n < 0) {
            perror("Receive failed");
            break;
        }
        buffer[n] = '\0';
        std::cout << "Получено сообщение: " << buffer << std::endl;
    }

    // Выход из многоадресной группы
    if (setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &multicastRequest, sizeof(multicastRequest)) < 0) {
        perror("Failed to leave multicast group");
    }

    close(sockfd);
    return 0;
}
