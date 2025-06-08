// bee_client_8.cpp
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>
#include <chrono>
#include <thread>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Использование: " << argv[0] << " <SERVER_IP> <SERVER_PORT> <BEE_SWARM_ID>" << std::endl;
        return 1;
    }

    const char* serverIP = argv[1];
    int serverPort = std::stoi(argv[2]);
    int swarmId = std::stoi(argv[3]);

    // Создание UDP сокета
    int sockfd;
    struct sockaddr_in serverAddr;
    socklen_t addrLen = sizeof(serverAddr);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Ошибка создания сокета");
        return 1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);

    std::cout << "Стая пчел #" << swarmId << " готова к поиску Винни-Пуха!" << std::endl;

    // Генератор случайных чисел для симуляции времени поиска
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> searchTime(1, 3); // Время поиска в секундах

    bool searching = true;
    while (searching) {
        // Запрос у улья сектор для поиска с указанием ID стаи
        char request[32];
        sprintf(request, "REQUEST:%d", swarmId);
        sendto(sockfd, request, strlen(request), 0, (struct sockaddr*)&serverAddr, addrLen);
        
        // Получение ответа от улья
        char buffer[1024] = {0};
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&serverAddr, &addrLen);
        if (n < 0) {
            perror("Ошибка приема");
            continue;
        }

        if (strncmp(buffer, "SEARCH:", 7) == 0) {
            // Получен сектор для поиска
            int sectorId;
            sscanf(buffer + 7, "%d", &sectorId);
            
            std::cout << "Стая #" << swarmId << ": Исследуем сектор " << sectorId << "..." << std::endl;
            
            // Симуляция поиска
            int duration = searchTime(gen);
            std::this_thread::sleep_for(std::chrono::seconds(duration));
            
            // Отправляем отчет улью с ID стаи и номером сектора
            char report[32];
            sprintf(report, "REPORT:%d:%d:", swarmId, sectorId);
            sendto(sockfd, report, strlen(report), 0, (struct sockaddr*)&serverAddr, addrLen);
            
            // Получаем дальнейшие инструкции
            n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&serverAddr, &addrLen);
            if (n < 0) {
                perror("Ошибка приема");
                continue;
            }
            
            if (strcmp(buffer, "WINNIE_FOUND") == 0) {
                std::cout << "Стая #" << swarmId << ": Мы нашли Винни-Пуха в секторе " << sectorId << "!" << std::endl;
                std::cout << "Стая #" << swarmId << ": Провели показательное наказание!" << std::endl;
                std::cout << "Стая #" << swarmId << ": Получена информация, что Винни-Пух найден и наказан. Возвращаемся в улей." << std::endl;
                searching = false;
            } else if (strcmp(buffer, "CONTINUE") == 0) {
                std::cout << "Стая #" << swarmId << ": Сектор " << sectorId << " проверен, Винни-Пух не обнаружен." << std::endl;
                std::cout << "Стая #" << swarmId << ": Продолжаем поиск." << std::endl;
            }
        } else if (strcmp(buffer, "NO_MORE_SECTORS") == 0) {
            std::cout << "Стая #" << swarmId << ": Все сектора проверены. Возвращаемся в улей." << std::endl;
            searching = false;
        } else if (strcmp(buffer, "WINNIE_FOUND") == 0) {
            std::cout << "Стая #" << swarmId << ": Получена информация, что Винни-Пух найден другой стаей. Возвращаемся в улей." << std::endl;
            searching = false;
        }
        
        // Добавляем паузу между запросами
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Стая #" << swarmId << ": Задание выполнено, возвращаемся в улей." << std::endl;
    
    close(sockfd);
    return 0;
}
