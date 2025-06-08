// bee_client_5.cpp - клиент (стая пчел) для задачи о Винни-Пухе (оценка 4-5)
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>
#include <chrono>
#include <thread>
#include <signal.h>

#define MIN_SEARCH_TIME 2    // Минимальное время поиска в секторе (сек)
#define MAX_SEARCH_TIME 5    // Максимальное время поиска в секторе (сек)

// Глобальная переменная для обработки сигналов
volatile bool running = true;

// Обработчик сигналов
void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        std::cout << "\nПолучен сигнал завершения. Завершаем работу..." << std::endl;
        running = false;
    }
}

// Функция для симуляции поиска Винни-Пуха в секторе
void searchForWinnieInSector(int sectorId, int swarmId) {
    std::cout << "Стая #" << swarmId << ": Начинаем поиск в секторе " << sectorId << "..." << std::endl;
    
    // Генерируем случайное время поиска
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> searchTimeDist(MIN_SEARCH_TIME, MAX_SEARCH_TIME);
    int searchTime = searchTimeDist(gen);
    
    // Симулируем процесс поиска
    for (int i = 0; i < searchTime && running; i++) {
        std::cout << "Стая #" << swarmId << ": Исследуем сектор " << sectorId 
                  << "... [" << (i+1) << "/" << searchTime << "]" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "Стая #" << swarmId << ": Поиск в секторе " << sectorId << " завершен" << std::endl;
}

int main(int argc, char* argv[]) {
    // Проверка аргументов командной строки
    if (argc != 4) {
        std::cerr << "Использование: " << argv[0] << " <SERVER_IP> <SERVER_PORT> <SWARM_ID>" << std::endl;
        return 1;
    }

    // Установка обработчиков сигналов
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    const char* serverIP = argv[1];
    int serverPort = std::stoi(argv[2]);
    int swarmId = std::stoi(argv[3]);

    // Создание UDP сокета
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Ошибка создания сокета");
        return 1;
    }

    // Настройка адреса сервера
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);
    serverAddr.sin_port = htons(serverPort);
    socklen_t addrLen = sizeof(serverAddr);

    std::cout << "Стая #" << swarmId << ": Готова к поиску Винни-Пуха!" << std::endl;
    std::cout << "Подключение к серверу " << serverIP << ":" << serverPort << std::endl;

    bool searching = true;
    while (running && searching) {
        // Запрос у сервера сектора для поиска
        char request[32];
        sprintf(request, "REQUEST:%d", swarmId);
        
        std::cout << "Стая #" << swarmId << ": Запрашиваем сектор для поиска" << std::endl;
        sendto(sockfd, request, strlen(request), 0, (struct sockaddr*)&serverAddr, addrLen);
        
        // Получение ответа от сервера
        char buffer[1024] = {0};
        
        // Устанавливаем таймаут для recvfrom
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&serverAddr, &addrLen);
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cout << "Стая #" << swarmId << ": Таймаут при ожидании ответа от сервера" << std::endl;
                
                // Проверяем, не нужно ли завершать работу
                if (!running) {
                    break;
                }
                
                std::cout << "Стая #" << swarmId << ": Повторная попытка..." << std::endl;
                continue;
            } else {
                perror("Ошибка приема");
                break;
            }
        }

        // Обрабатываем ответ от сервера
        buffer[n] = '\0';
        
        if (strncmp(buffer, "SEARCH:", 7) == 0) {
            // Получен сектор для поиска
            int sectorId;
            sscanf(buffer + 7, "%d", &sectorId);
            
            std::cout << "Стая #" << swarmId << ": Получен сектор " << sectorId << " для поиска" << std::endl;
            
            // Симуляция полета к сектору
            std::cout << "Стая #" << swarmId << ": Летим к сектору " << sectorId << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // Симуляция поиска в секторе
            searchForWinnieInSector(sectorId, swarmId);
            
            if (!running) {
                break;
            }
            
            // Отправка отчета серверу
            char report[32];
            sprintf(report, "REPORT:%d:%d:", swarmId, sectorId);
            
            std::cout << "Стая #" << swarmId << ": Отправляем отчет о секторе " << sectorId << std::endl;
            sendto(sockfd, report, strlen(report), 0, (struct sockaddr*)&serverAddr, addrLen);
            
            // Получение инструкций от сервера
            memset(buffer, 0, sizeof(buffer));
            n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&serverAddr, &addrLen);
            
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::cout << "Стая #" << swarmId << ": Таймаут при ожидании инструкций" << std::endl;
                    continue;
                } else {
                    perror("Ошибка приема");
                    break;
                }
            }
            
            buffer[n] = '\0';
            
            if (strcmp(buffer, "WINNIE_FOUND") == 0) {
                // Мы нашли Винни-Пуха!
                std::cout << "Стая #" << swarmId << ": Мы нашли Винни-Пуха в секторе " << sectorId << "!" << std::endl;
                std::cout << "Стая #" << swarmId << ": Проводим показательное наказание!" << std::endl;
                std::cout << "Стая #" << swarmId << ": Возвращаемся в улей с чувством выполненного долга." << std::endl;
                searching = false;
            } else if (strcmp(buffer, "CONTINUE") == 0) {
                // Продолжаем поиск
                std::cout << "Стая #" << swarmId << ": Винни-Пух в секторе " << sectorId << " не обнаружен" << std::endl;
                std::cout << "Стая #" << swarmId << ": Возвращаемся в улей за новым заданием" << std::endl;
            }
            
        } else if (strcmp(buffer, "NO_MORE_SECTORS") == 0) {
            // Нет больше секторов для поиска
            std::cout << "Стая #" << swarmId << ": Все секторы уже проверены. Возвращаемся в улей" << std::endl;
            searching = false;
        } else if (strcmp(buffer, "WINNIE_FOUND") == 0) {
            // Другая стая нашла Винни-Пуха
            std::cout << "Стая #" << swarmId << ": Получена информация, что Винни-Пух уже найден и наказан другой стаей" << std::endl;
            std::cout << "Стая #" << swarmId << ": Возвращаемся в улей" << std::endl;
            searching = false;
        }
        
        // Небольшая пауза между запросами
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Сообщаем серверу о завершении работы
    if (sockfd >= 0) {
        char finishMsg[] = "FINISH";
        sendto(sockfd, finishMsg, strlen(finishMsg), 0, (struct sockaddr*)&serverAddr, addrLen);
        
        // Получаем подтверждение
        char buffer[1024] = {0};
        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&serverAddr, &addrLen);
        
        close(sockfd);
    }
    
    std::cout << "Стая #" << swarmId << ": Работа завершена" << std::endl;
    return 0;
}
