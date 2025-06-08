// bee_server_8.cpp
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <random>
#include <map>
#include <set>
#include <string>
#include <fcntl.h>
#include <sstream>

#define MAX_SECTORS 10 // Максимальное количество участков леса
#define MONITOR_PORT_OFFSET 1000 // Смещение порта для монитора

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

// Структура для хранения информации о подключенном мониторе
struct Monitor {
    std::string ip;
    int port;
    
    // Перегрузка оператора для использования в set
    bool operator<(const Monitor& other) const {
        if (ip != other.ip) return ip < other.ip;
        return port < other.port;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <IP> <PORT>" << std::endl;
        return 1;
    }

    const char* serverIP = argv[1];
    int serverPort = std::stoi(argv[2]);
    int monitorPort = serverPort + MONITOR_PORT_OFFSET;

    // Инициализация секторов леса
    std::vector<Sector> sectors;
    for (int i = 0; i < MAX_SECTORS; i++) {
        sectors.push_back({i, false, false});
    }

    // Случайно размещаем Винни-Пуха в одном из секторов
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, MAX_SECTORS - 1);
    int winnieSector = distrib(gen);
    sectors[winnieSector].winnieFound = true;

    std::cout << "Сервер: Винни-Пух находится в секторе " << winnieSector << std::endl;

    // Создание UDP сокета для пчел
    int sockfd;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Ошибка создания сокета для пчел");
        return 1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);
    serverAddr.sin_port = htons(serverPort);

    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Ошибка привязки сокета для пчел");
        close(sockfd);
        return 1;
    }

    // Создание UDP сокета для мониторов
    int monitorSockfd;
    struct sockaddr_in monitorServerAddr;

    if ((monitorSockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Ошибка создания сокета для мониторов");
        close(sockfd);
        return 1;
    }

    memset(&monitorServerAddr, 0, sizeof(monitorServerAddr));
    monitorServerAddr.sin_family = AF_INET;
    monitorServerAddr.sin_addr.s_addr = inet_addr(serverIP);
    monitorServerAddr.sin_port = htons(monitorPort);

    if (bind(monitorSockfd, (struct sockaddr*)&monitorServerAddr, sizeof(monitorServerAddr)) < 0) {
        perror("Ошибка привязки сокета для мониторов");
        close(sockfd);
        close(monitorSockfd);
        return 1;
    }

    std::cout << "Сервер запущен на " << serverIP << ":" << serverPort << std::endl;
    std::cout << "Порт для мониторов: " << monitorPort << std::endl;
    std::cout << "Ожидание пчел и мониторов..." << std::endl;

    // Для отслеживания активных стай и мониторов
    std::map<int, BeeSwarm> beeSwarms;
    std::set<Monitor> monitors; // Множество подключенных мониторов
    bool winnieFoundByBees = false;
    bool allSectorsSearched = false;

    // Неблокирующий режим для мониторинга обоих сокетов
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(monitorSockfd, F_GETFL, 0);
    fcntl(monitorSockfd, F_SETFL, flags | O_NONBLOCK);

    while (!winnieFoundByBees && !allSectorsSearched) {
        // Проверяем сообщения от пчел
        char buffer[1024] = {0};
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&clientAddr, &addrLen);
        
        if (n > 0) {
            int swarmId;
            
            // Обработка запроса от стаи пчел
            if (sscanf(buffer, "REQUEST:%d", &swarmId) == 1) {
                // Поиск неисследованного сектора
                int sectorToSearch = -1;
                for (int i = 0; i < MAX_SECTORS; i++) {
                    if (!sectors[i].searched) {
                        sectorToSearch = i;
                        sectors[i].searched = true;
                        break;
                    }
                }

                if (sectorToSearch == -1) {
                    // Все секторы исследованы
                    char response[] = "NO_MORE_SECTORS";
                    sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientAddr, addrLen);
                    
                    // Проверяем, исследованы ли все секторы
                    allSectorsSearched = true;
                    for (const auto& sector : sectors) {
                        if (!sector.searched) {
                            allSectorsSearched = false;
                            break;
                        }
                    }

                    std::cout << "Сервер: Все секторы исследованы" << std::endl;
                    
                    // Обновляем статус стаи
                    if (beeSwarms.find(swarmId) != beeSwarms.end()) {
                        beeSwarms[swarmId].active = false;
                        beeSwarms[swarmId].currentSector = -1;
                    }
                } else {
                    // Отправляем номер сектора для исследования
                    char response[32];
                    sprintf(response, "SEARCH:%d", sectorToSearch);
                    sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientAddr, addrLen);
                    std::cout << "Сервер: Стая #" << swarmId << " направлена в сектор " << sectorToSearch << std::endl;
                    
                    // Обновляем или добавляем информацию о стае
                    beeSwarms[swarmId] = {swarmId, sectorToSearch, true};
                }
            } else if (sscanf(buffer, "REPORT:%d:%d:", &swarmId, &n) == 2) {
                // Получаем отчет от пчел
                int sectorId;
                sscanf(buffer, "REPORT:%d:%d:", &swarmId, &sectorId);
                
                // Проверяем, находится ли Винни-Пух в этом секторе
                bool isWinnieInSector = false;
                for (auto& sector : sectors) {
                    if (sector.id == sectorId && sector.winnieFound) {
                        isWinnieInSector = true;
                        break;
                    }
                }
                
                if (isWinnieInSector) {
                    std::cout << "Сервер: Стая пчел #" << swarmId << " сообщает, что Винни-Пух найден в секторе " << sectorId << " и наказан!" << std::endl;
                    winnieFoundByBees = true;
                    
                    // Обновляем информацию о секторе
                    for (auto& sector : sectors) {
                        if (sector.id == sectorId) {
                            sector.searched = true;
                            sector.winnieFound = true;
                            break;
                        }
                    }
                    
                    // Информируем всех пчел
                    char response[] = "WINNIE_FOUND";
                    sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientAddr, addrLen);
                    
                    // Обновляем статус стаи
                    if (beeSwarms.find(swarmId) != beeSwarms.end()) {
                        beeSwarms[swarmId].active = false;
                        beeSwarms[swarmId].currentSector = -1;
                    }
                } else {
                    std::cout << "Сервер: Стая пчел #" << swarmId << " сообщает, что сектор " << sectorId << " проверен, Винни-Пух не обнаружен" << std::endl;
                    char response[] = "CONTINUE";
                    sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientAddr, addrLen);
                    
                    // Обновляем статус стаи
                    if (beeSwarms.find(swarmId) != beeSwarms.end()) {
                        beeSwarms[swarmId].active = true;
                        beeSwarms[swarmId].currentSector = -1; // Стая вернулась в улей
                    }
                }
            }
        }
        
        // Проверяем сообщения от мониторов
        struct sockaddr_in monitorAddr;
        socklen_t monitorAddrLen = sizeof(monitorAddr);
        memset(buffer, 0, sizeof(buffer));
        n = recvfrom(monitorSockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&monitorAddr, &monitorAddrLen);
        
        if (n > 0) {
            std::string monitorIP = inet_ntoa(monitorAddr.sin_addr);
            int monitorPort = ntohs(monitorAddr.sin_port);
            Monitor currentMonitor = {monitorIP, monitorPort};
            
            if (strcmp(buffer, "CONNECT_MONITOR") == 0) {
                // Добавляем новый монитор
                monitors.insert(currentMonitor);
                std::cout << "Сервер: Монитор подключен с " << monitorIP << ":" << monitorPort << std::endl;
                std::cout << "Сервер: Всего мониторов: " << monitors.size() << std::endl;
                
                // Отправляем информацию о состоянии игры новому монитору
                char response[1024];
                sprintf(response, "INIT:%d:%d", MAX_SECTORS, winnieSector);
                sendto(monitorSockfd, response, strlen(response), 0, (struct sockaddr*)&monitorAddr, monitorAddrLen);
            } else if (strcmp(buffer, "STATUS") == 0) {
                // Проверяем, что монитор зарегистрирован
                if (monitors.find(currentMonitor) != monitors.end()) {
                    // Отправка текущего статуса
                    std::string status = "STATUS:";
                    
                    // Добавляем информацию о секторах
                    for (const auto& sector : sectors) {
                        status += std::to_string(sector.id) + ":" + 
                                (sector.searched ? "1:" : "0:") + 
                                (sector.winnieFound && sector.searched ? "1:" : "0:");
                    }
                    
                    status += "BEES:";
                    
                    // Добавляем информацию о стаях
                    for (const auto& [id, swarm] : beeSwarms) {
                        status += std::to_string(swarm.id) + ":" + 
                                std::to_string(swarm.currentSector) + ":" + 
                                (swarm.active ? "1:" : "0:");
                    }
                    
                    // Добавляем общий статус
                    status += "GAME:";
                    status += std::string(winnieFoundByBees ? "1:" : "0:") + 
                            std::string(allSectorsSearched ? "1" : "0");
                    
                    sendto(monitorSockfd, status.c_str(), status.length(), 0, 
                        (struct sockaddr*)&monitorAddr, monitorAddrLen);
                }
            } else if (strcmp(buffer, "DISCONNECT_MONITOR") == 0) {
                // Удаляем монитор из списка
                monitors.erase(currentMonitor);
                std::cout << "Сервер: Монитор отключен с " << monitorIP << ":" << monitorPort << std::endl;
                std::cout << "Сервер: Всего мониторов: " << monitors.size() << std::endl;
            }
        }
        
        // Небольшая пауза для уменьшения нагрузки на CPU
        usleep(10000); // 10 мс
    }

    std::cout << "Сервер: Поиск завершен!" << std::endl;
    if (winnieFoundByBees) {
        std::cout << "Сервер: Винни-Пух был найден и наказан!" << std::endl;
    } else {
        std::cout << "Сервер: Все секторы проверены, но Винни-Пух не найден." << std::endl;
    }
    
    // Отправляем финальный статус всем мониторам
    std::string finalStatus = "FINAL:";
    finalStatus += std::string(winnieFoundByBees ? "1:" : "0:") + 
                 std::string(allSectorsSearched ? "1" : "0");

    for (const auto& monitor : monitors) {
        struct sockaddr_in monitorAddr;
        memset(&monitorAddr, 0, sizeof(monitorAddr));
        monitorAddr.sin_family = AF_INET;
        monitorAddr.sin_addr.s_addr = inet_addr(monitor.ip.c_str());
        monitorAddr.sin_port = htons(monitor.port);
        
        sendto(monitorSockfd, finalStatus.c_str(), finalStatus.length(), 0, 
              (struct sockaddr*)&monitorAddr, sizeof(monitorAddr));
    }

    close(sockfd);
    close(monitorSockfd);
    return 0;
}
