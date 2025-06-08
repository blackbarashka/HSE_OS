// bee_monitor_8.cpp
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include <signal.h>

struct Sector {
    int id;
    bool searched = false;
    bool winnieFound = false;
};

struct BeeSwarm {
    int id;
    int currentSector = -1;
    bool active = false;
};

// Для обработки Ctrl+C и корректного выхода
volatile bool running = true;
int sockfd = -1;
struct sockaddr_in serverAddr;
socklen_t addrLen;

void signalHandler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\nОтключение от сервера..." << std::endl;
        // Отправляем сообщение об отключении
        if (sockfd >= 0) {
            char disconnectMsg[] = "DISCONNECT_MONITOR";
            sendto(sockfd, disconnectMsg, strlen(disconnectMsg), 0, (struct sockaddr*)&serverAddr, addrLen);
        }
        running = false;
    }
}

void clearScreen() {
    std::cout << "\033[2J\033[1;1H"; // ANSI escape sequences для очистки экрана
}

void displayForest(const std::vector<Sector>& sectors, const std::map<int, BeeSwarm>& beeSwarms, int totalSectors, int winnieSector, int monitorId) {
    clearScreen();
    
    std::cout << "=== Монитор #" << monitorId << " поиска Винни-Пуха ===" << std::endl;
    std::cout << "Лес разделен на " << totalSectors << " секторов" << std::endl;
    
    // Отображение статуса секторов
    std::cout << "\nСтатус секторов:" << std::endl;
    for (int i = 0; i < totalSectors; i++) {
        std::string status = "[ ]";
        std::string beeInfo = "";
        
        // Находим сектор в списке
        for (const auto& sector : sectors) {
            if (sector.id == i) {
                if (sector.searched) {
                    if (sector.winnieFound) {
                        status = "[W]"; // Винни-Пух найден
                    } else {
                        status = "[X]"; // Сектор проверен, но Винни-Пух не найден
                    }
                }
                break;
            }
        }
        
        // Проверяем, есть ли стаи в этом секторе
        for (const auto& [id, swarm] : beeSwarms) {
            if (swarm.currentSector == i && swarm.active) {
                beeInfo += " 🐝#" + std::to_string(swarm.id);
            }
        }
        
        std::cout << "Сектор " << i << ": " << status << beeInfo << std::endl;
    }
    
    // Отображение статуса стай
    std::cout << "\nАктивные стаи пчел:" << std::endl;
    bool anyActive = false;
    for (const auto& [id, swarm] : beeSwarms) {
        if (swarm.active) {
            anyActive = true;
            std::cout << "Стая #" << swarm.id;
            if (swarm.currentSector >= 0) {
                std::cout << " - исследует сектор " << swarm.currentSector;
            } else {
                std::cout << " - возвращается в улей";
            }
            std::cout << std::endl;
        }
    }
    
    if (!anyActive) {
        std::cout << "Нет активных стай" << std::endl;
    }
    
    std::cout << "\n== Нажмите Ctrl+C для отключения от сервера ==" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Использование: " << argv[0] << " <SERVER_IP> <MONITOR_PORT> [MONITOR_ID]" << std::endl;
        return 1;
    }

    const char* serverIP = argv[1];
    int monitorPort = std::stoi(argv[2]);
    int monitorId = (argc == 4) ? std::stoi(argv[3]) : 0;

    // Установка обработчика сигнала для корректного выхода
    signal(SIGINT, signalHandler);

    // Создание UDP сокета
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Ошибка создания сокета");
        return 1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(monitorPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);
    addrLen = sizeof(serverAddr);

    // Устанавливаем таймаут для recvfrom
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::cout << "Монитор #" << monitorId << " поиска Винни-Пуха запущен. Подключаемся к серверу..." << std::endl;
    
    // Подключение к серверу
    char connectMsg[] = "CONNECT_MONITOR";
    sendto(sockfd, connectMsg, strlen(connectMsg), 0, (struct sockaddr*)&serverAddr, addrLen);
    
    // Получение начальной информации от сервера
    char buffer[1024] = {0};
    int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&serverAddr, &addrLen);
    if (n < 0) {
        std::cerr << "Ошибка подключения к серверу или таймаут соединения" << std::endl;
        std::cerr << "Убедитесь, что сервер запущен на " << serverIP << ":" << monitorPort << std::endl;
        close(sockfd);
        return 1;
    }
    
    int totalSectors = 0;
    int winnieSector = -1;
    
    // Обработка начальной информации
    if (strncmp(buffer, "INIT:", 5) == 0) {
        sscanf(buffer + 5, "%d:%d", &totalSectors, &winnieSector);
        std::cout << "Подключено к серверу. Лес разделен на " << totalSectors << " секторов." << std::endl;
    } else {
        std::cerr << "Неверный формат начальной информации: " << buffer << std::endl;
        close(sockfd);
        return 1;
    }
    
    std::vector<Sector> sectors;
    std::map<int, BeeSwarm> beeSwarms;
    bool gameOver = false;
    
    // Основной цикл мониторинга
    while (running && !gameOver) {
        // Запрос обновленного статуса у сервера
        char statusReq[] = "STATUS";
        sendto(sockfd, statusReq, strlen(statusReq), 0, (struct sockaddr*)&serverAddr, addrLen);
        
        // Получение ответа
        memset(buffer, 0, sizeof(buffer));
        n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&serverAddr, &addrLen);
        
        if (n > 0) {
            if (strncmp(buffer, "STATUS:", 7) == 0) {
                // Парсим сообщение
                std::string message(buffer);
                std::stringstream ss(message.substr(7));
                std::string token;
                
                // Очищаем предыдущие данные
                sectors.clear();
                beeSwarms.clear();
                
                // Парсим информацию о секторах до маркера "BEES:"
                while (std::getline(ss, token, ':')) {
                    if (token == "BEES") break;
                    
                    int sectorId = std::stoi(token);
                    bool searched = false;
                    bool winnieFound = false;
                    
                    if (std::getline(ss, token, ':')) searched = (token == "1");
                    if (std::getline(ss, token, ':')) winnieFound = (token == "1");
                    
                    sectors.push_back({sectorId, searched, winnieFound});
                }
                
                // Парсим информацию о стаях пчел до маркера "GAME:"
                while (std::getline(ss, token, ':')) {
                    if (token == "GAME") break;
                    
                    int swarmId = std::stoi(token);
                    int currentSector = -1;
                    bool active = false;
                    
                    if (std::getline(ss, token, ':')) currentSector = std::stoi(token);
                    if (std::getline(ss, token, ':')) active = (token == "1");
                    
                    beeSwarms[swarmId] = {swarmId, currentSector, active};
                }
                
                // Проверяем статус игры
                bool winnieFoundByBees = false;
                bool allSectorsSearched = false;
                
                if (std::getline(ss, token, ':')) winnieFoundByBees = (token == "1");
                if (std::getline(ss, token, ':')) allSectorsSearched = (token == "1");
                
                // Отображаем текущий статус
                displayForest(sectors, beeSwarms, totalSectors, winnieSector, monitorId);
                
                // Проверяем конец игры
                gameOver = winnieFoundByBees || allSectorsSearched;
                
            } else if (strncmp(buffer, "FINAL:", 6) == 0) {
                // Игра завершена
                bool winnieFound = false;
                bool allSearched = false;
                
                sscanf(buffer + 6, "%d:%d", &winnieFound, &allSearched);
                
                clearScreen();
                std::cout << "=== Монитор #" << monitorId << ": Поиск Винни-Пуха завершен! ===" << std::endl;
                
                if (winnieFound) {
                    std::cout << "Винни-Пух был найден и наказан!" << std::endl;
                } else if (allSearched) {
                    std::cout << "Все секторы проверены, но Винни-Пух не найден." << std::endl;
                }
                
                gameOver = true;
            }
        }
        
        // Пауза перед следующим обновлением
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Отправляем сообщение об отключении при выходе
    if (sockfd >= 0 && !gameOver) {
        char disconnectMsg[] = "DISCONNECT_MONITOR";
        sendto(sockfd, disconnectMsg, strlen(disconnectMsg), 0, (struct sockaddr*)&serverAddr, addrLen);
        std::cout << "Мониторинг завершен. Отключение от сервера." << std::endl;
    } else {
        std::cout << "Мониторинг завершен." << std::endl;
    }
    
    close(sockfd);
    return 0;
}

