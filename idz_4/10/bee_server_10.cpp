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
#include <signal.h>
#include <mutex>
#include <thread>
#include <chrono>

#define MAX_SECTORS 10 // Максимальное количество участков леса
#define MONITOR_PORT_OFFSET 1000 // Смещение порта для монитора
#define CONTROL_PORT_OFFSET 2000 // Смещение порта для управляющего интерфейса
#define HEARTBEAT_INTERVAL 5 // Интервал проверки активности клиентов (сек)
#define CLIENT_TIMEOUT 15 // Таймаут для определения отключения клиента (сек)

// Структура для хранения информации о секторе
struct Sector {
    int id;
    bool searched = false;
    bool assigned = false;  // Сектор назначен для поиска
    bool winnieFound = false;
};

// Структура для хранения информации о стае пчел
struct BeeSwarm {
    int id;
    int currentSector = -1;
    bool active = false;
    bool searchInProgress = false;  // Флаг, указывающий что стая выполняет поиск
    bool disconnected = false;
    std::string ip;
    int port;
    time_t lastSeen; // Время последнего контакта
};

// Структура для хранения информации о мониторе
struct Monitor {
    std::string ip;
    int port;
    time_t lastSeen; // Время последнего контакта
    
    // Перегрузка оператора для использования в set
    bool operator<(const Monitor& other) const {
        if (ip != other.ip) return ip < other.ip;
        return port < other.port;
    }
};

// Глобальные переменные для обработки сигналов
volatile bool running = true;
int sockfd = -1;
int monitorSockfd = -1;

// Мьютексы для синхронизации доступа к общим данным
std::mutex sectorsMutex;
std::mutex swarmsMutex;
std::mutex monitorsMutex;

std::vector<Sector> sectors;
std::map<int, BeeSwarm> beeSwarms;
std::set<Monitor> monitors;

int winnieSector = -1;
bool winnieFoundByBees = false;
bool allSectorsSearched = false;

// Обработчик сигналов для корректного завершения
void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        std::cout << "\nПолучен сигнал завершения. Завершение работы сервера и клиентов..." << std::endl;
        running = false;
    }
}

// Функция для отправки сообщения о завершении всем клиентам
void notifyClientsServerShutdown() {
    if (sockfd < 0 || monitorSockfd < 0) return;
    
    // Отправляем сообщение всем пчёлам
    {
        std::lock_guard<std::mutex> lock(swarmsMutex);
        for (const auto& [id, swarm] : beeSwarms) {
            if (!swarm.disconnected && swarm.active) {
                struct sockaddr_in clientAddr;
                memset(&clientAddr, 0, sizeof(clientAddr));
                clientAddr.sin_family = AF_INET;
                clientAddr.sin_addr.s_addr = inet_addr(swarm.ip.c_str());
                clientAddr.sin_port = htons(swarm.port);
                
                char shutdownMsg[] = "SERVER_SHUTDOWN";
                sendto(sockfd, shutdownMsg, strlen(shutdownMsg), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
                std::cout << "Сервер: Отправлен сигнал завершения стае #" << id << std::endl;
            }
        }
    }
    
    // Отправляем сообщение всем мониторам
    {
        std::lock_guard<std::mutex> monLock(monitorsMutex);
        for (const auto& monitor : monitors) {
            struct sockaddr_in monitorAddr;
            memset(&monitorAddr, 0, sizeof(monitorAddr));
            monitorAddr.sin_family = AF_INET;
            monitorAddr.sin_addr.s_addr = inet_addr(monitor.ip.c_str());
            monitorAddr.sin_port = htons(monitor.port);
            
            char shutdownMsg[] = "SERVER_SHUTDOWN";
            sendto(monitorSockfd, shutdownMsg, strlen(shutdownMsg), 0, (struct sockaddr*)&monitorAddr, sizeof(monitorAddr));
            std::cout << "Сервер: Отправлен сигнал завершения монитору с " << monitor.ip << ":" << monitor.port << std::endl;
        }
    }
    
    // Даем клиентам немного времени для обработки сообщения
    std::cout << "Сервер: Ожидание завершения работы клиентов..." << std::endl;
    sleep(2);
}

// Функция для отправки сообщения о нахождении Винни-Пуха всем стаям
void notifyAllSwarmsWinnieFound() {
    if (sockfd < 0) return;
    
    std::lock_guard<std::mutex> lock(swarmsMutex);
    for (auto& [id, swarm] : beeSwarms) {
        if (!swarm.disconnected && swarm.active) {
            struct sockaddr_in clientAddr;
            memset(&clientAddr, 0, sizeof(clientAddr));
            clientAddr.sin_family = AF_INET;
            clientAddr.sin_addr.s_addr = inet_addr(swarm.ip.c_str());
            clientAddr.sin_port = htons(swarm.port);
            
            // Отправляем сообщение о находке, если стая не является той, которая нашла Винни-Пуха
            if (swarm.searchInProgress) {
                char foundMsg[] = "WINNIE_FOUND";
                sendto(sockfd, foundMsg, strlen(foundMsg), 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
                std::cout << "Сервер: Отправлено уведомление о находке Винни-Пуха стае #" << id << std::endl;
            }
            
            // Снимаем флаг поиска для всех стай
            swarm.searchInProgress = false;
        }
    }
    
    // Освобождаем все назначенные, но не исследованные сектора
    {
        std::lock_guard<std::mutex> sectorsLock(sectorsMutex);
        for (auto& sector : sectors) {
            if (sector.assigned && !sector.searched) {
                sector.assigned = false;
            }
        }
    }
}

// Функция для проверки активности клиентов
void checkClientsActivity() {
    time_t currentTime = time(nullptr);
    
    // Проверяем активность пчёл
    {
        std::lock_guard<std::mutex> lock(swarmsMutex);
        std::vector<int> inactiveSwarms;
        
        for (auto& [id, swarm] : beeSwarms) {
            if (!swarm.disconnected && swarm.active && 
                difftime(currentTime, swarm.lastSeen) > CLIENT_TIMEOUT) {
                std::cout << "Сервер: Стая #" << swarm.id << " не отвечает и будет помечена как отключенная" << std::endl;
                swarm.disconnected = true;
                swarm.active = false;
                inactiveSwarms.push_back(id);
                
                // Освобождаем сектор, если стая находилась в поиске
                if (swarm.searchInProgress && swarm.currentSector >= 0) {
                    std::lock_guard<std::mutex> sectorsLock(sectorsMutex);
                    for (auto& sector : sectors) {
                        if (sector.id == swarm.currentSector) {
                            sector.assigned = false;  // Освобождаем сектор для других стай
                            break;
                        }
                    }
                    swarm.searchInProgress = false;
                    swarm.currentSector = -1;
                }
            }
        }
    }
    
    // Проверяем активность мониторов
    {
        std::lock_guard<std::mutex> lock(monitorsMutex);
        std::vector<Monitor> inactiveMonitors;
        
        for (const auto& monitor : monitors) {
            if (difftime(currentTime, monitor.lastSeen) > CLIENT_TIMEOUT) {
                inactiveMonitors.push_back(monitor);
            }
        }
        
        for (const auto& monitor : inactiveMonitors) {
            monitors.erase(monitor);
            std::cout << "Сервер: Монитор с " << monitor.ip << ":" << monitor.port << " не отвечает и будет удален" << std::endl;
        }
    }
}

// Поток для обработки запросов управления
void controlThreadFunction(const char* serverIP, int controlPort) {
    // Создаем UDP сокет для управления
    int controlSockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (controlSockfd < 0) {
        perror("Ошибка создания сокета для управления");
        return;
    }

    // Настраиваем адрес для привязки
    struct sockaddr_in controlAddr;
    memset(&controlAddr, 0, sizeof(controlAddr));
    controlAddr.sin_family = AF_INET;
    controlAddr.sin_addr.s_addr = inet_addr(serverIP);
    controlAddr.sin_port = htons(controlPort);

    // Привязываем сокет
    if (bind(controlSockfd, (struct sockaddr*)&controlAddr, sizeof(controlAddr)) < 0) {
        perror("Ошибка привязки сокета для управления");
        close(controlSockfd);
        return;
    }

    std::cout << "Поток управления запущен на порту " << controlPort << std::endl;

    // Устанавливаем неблокирующий режим
    int flags = fcntl(controlSockfd, F_GETFL, 0);
    fcntl(controlSockfd, F_SETFL, flags | O_NONBLOCK);

    char buffer[1024];
    struct sockaddr_in clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    while (running) {
        memset(buffer, 0, sizeof(buffer));
        
        int n = recvfrom(controlSockfd, buffer, sizeof(buffer) - 1, 0,
                         (struct sockaddr*)&clientAddr, &clientAddrLen);
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Нет данных, ждем
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            } else {
                perror("Ошибка приема в потоке управления");
                break;
            }
        }
        
        if (n > 0) {
            buffer[n] = '\0';
            std::string command(buffer);
            std::string response;
            
            if (command == "LIST_BEES") {
                // Команда для получения списка всех стай
                response = "BEES:";
                
                std::lock_guard<std::mutex> lock(swarmsMutex);
                for (const auto& [id, swarm] : beeSwarms) {
                    response += std::to_string(id) + ":" + 
                               (swarm.active ? "1:" : "0:") + 
                               (swarm.disconnected ? "1:" : "0:") + 
                               std::to_string(swarm.currentSector) + ":";
                }
                
            } else if (command.find("DISCONNECT_BEE:") == 0) {
                // Команда для отключения стаи
                int swarmId = std::stoi(command.substr(15));
                
                std::lock_guard<std::mutex> lock(swarmsMutex);
                if (beeSwarms.find(swarmId) != beeSwarms.end() && beeSwarms[swarmId].active) {
                    beeSwarms[swarmId].disconnected = true;
                    beeSwarms[swarmId].active = false;
                    
                    // Освобождаем сектор, если стая находилась в поиске
                    if (beeSwarms[swarmId].searchInProgress && beeSwarms[swarmId].currentSector >= 0) {
                        std::lock_guard<std::mutex> sectorsLock(sectorsMutex);
                        for (auto& sector : sectors) {
                            if (sector.id == beeSwarms[swarmId].currentSector) {
                                sector.assigned = false;  // Освобождаем сектор
                                break;
                            }
                        }
                        beeSwarms[swarmId].searchInProgress = false;
                        beeSwarms[swarmId].currentSector = -1;
                    }
                    
                    response = "OK:Стая #" + std::to_string(swarmId) + " отключена";
                    std::cout << "Управление: " << response << std::endl;
                } else {
                    response = "ERROR:Стая #" + std::to_string(swarmId) + " не найдена или уже отключена";
                }
                
            } else if (command.find("RECONNECT_BEE:") == 0) {
                // Команда для повторного подключения стаи
                int swarmId = std::stoi(command.substr(14));
                
                std::lock_guard<std::mutex> lock(swarmsMutex);
                if (beeSwarms.find(swarmId) != beeSwarms.end() && beeSwarms[swarmId].disconnected) {
                    beeSwarms[swarmId].disconnected = false;
                    beeSwarms[swarmId].active = false;  // Стая должна сама активироваться при подключении
                    
                    response = "OK:Стая #" + std::to_string(swarmId) + " готова к переподключению";
                    std::cout << "Управление: " << response << std::endl;
                } else {
                    response = "ERROR:Стая #" + std::to_string(swarmId) + " не найдена или не была отключена";
                }
                
            } else if (command == "STATUS") {
                // Команда для получения общего статуса
                response = "STATUS:";
                
                // Информация о секторах
                {
                    std::lock_guard<std::mutex> lock(sectorsMutex);
                    int searchedCount = 0;
                    for (const auto& sector : sectors) {
                        if (sector.searched) searchedCount++;
                    }
                    response += "SECTORS:" + std::to_string(MAX_SECTORS) + ":" + 
                               std::to_string(searchedCount) + ":";
                }
                
                // Информация о стаях
                {
                    std::lock_guard<std::mutex> lock(swarmsMutex);
                    int activeCount = 0, disconnectedCount = 0;
                    for (const auto& [id, swarm] : beeSwarms) {
                        if (swarm.active) activeCount++;
                        if (swarm.disconnected) disconnectedCount++;
                    }
                    response += "BEES:" + std::to_string(beeSwarms.size()) + ":" + 
                               std::to_string(activeCount) + ":" + 
                               std::to_string(disconnectedCount) + ":";
                }
                
                // Информация об игре
                response += "GAME:";
                response += winnieFoundByBees ? "1:" : "0:";
                response += allSectorsSearched ? "1" : "0";
                
            } else if (command == "HELP") {
                // Команда для получения списка доступных команд
                response = "COMMANDS:LIST_BEES - список стай;DISCONNECT_BEE:<id> - отключить стаю;RECONNECT_BEE:<id> - повторно подключить стаю;STATUS - получить общий статус;HELP - список команд";
            } else {
                response = "ERROR:Неизвестная команда. Используйте HELP для получения списка команд.";
            }
            
            // Отправляем ответ
            sendto(controlSockfd, response.c_str(), response.length(), 0,
                   (struct sockaddr*)&clientAddr, clientAddrLen);
        }
    }
    
    close(controlSockfd);
    std::cout << "Поток управления завершен." << std::endl;
}

// Поток для периодической проверки состояния
void monitoringThread() {
    while (running) {
        // Проверка активности клиентов
        checkClientsActivity();
        
        // Проверка, все ли секторы исследованы
        bool allSearched = true;
        {
            std::lock_guard<std::mutex> lock(sectorsMutex);
            for (const auto& sector : sectors) {
                if (!sector.searched) {
                    allSearched = false;
                    break;
                }
            }
        }
        
        if (allSearched) {
            allSectorsSearched = true;
            if (!winnieFoundByBees) {
                std::cout << "Мониторинг: Все секторы исследованы, но Винни-Пух не найден." << std::endl;
            }
        }
        
        // Пауза между проверками
        for (int i = 0; i < 5 && running; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    std::cout << "Поток мониторинга завершен." << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <IP> <PORT>" << std::endl;
        return 1;
    }

    // Установка обработчика сигналов для корректного завершения
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    const char* serverIP = argv[1];
    int serverPort = std::stoi(argv[2]);
    int monitorPort = serverPort + MONITOR_PORT_OFFSET;
    int controlPort = serverPort + CONTROL_PORT_OFFSET;

    // Инициализация секторов леса
    {
        std::lock_guard<std::mutex> lock(sectorsMutex);
        sectors.clear();
        for (int i = 0; i < MAX_SECTORS; i++) {
            sectors.push_back({i, false, false, false});
        }
        
        // Случайно размещаем Винни-Пуха в одном из секторов
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, MAX_SECTORS - 1);
        winnieSector = distrib(gen);
    }
    
    std::cout << "Сервер: Винни-Пух находится в секторе " << winnieSector << std::endl;

    // Создание UDP сокета для пчел
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Ошибка создания сокета для пчел");
        return 1;
    }

    struct sockaddr_in serverAddr;
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
    monitorSockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (monitorSockfd < 0) {
        perror("Ошибка создания сокета для мониторов");
        close(sockfd);
        return 1;
    }

    struct sockaddr_in monitorServerAddr;
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

    // Запуск потока управления для обработки команд
    std::thread controlThread(controlThreadFunction, serverIP, controlPort);
    
    // Запуск потока мониторинга
    std::thread monitThread(monitoringThread);

    std::cout << "Сервер запущен на " << serverIP << ":" << serverPort << std::endl;
    std::cout << "Порт для мониторов: " << monitorPort << std::endl;
    std::cout << "Порт для управления: " << controlPort << std::endl;
    std::cout << "Ожидание пчел и мониторов..." << std::endl;

    // Неблокирующий режим для мониторинга обоих сокетов
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(monitorSockfd, F_GETFL, 0);
    fcntl(monitorSockfd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    
    struct sockaddr_in monitorAddr;
    socklen_t monitorAddrLen = sizeof(monitorAddr);

    // Основной цикл сервера
    while (running && (!allSectorsSearched || !winnieFoundByBees)) {
        char buffer[1024] = {0};
        
        // Проверяем сообщения от пчел
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&clientAddr, &addrLen);
        
        if (n > 0) {
            buffer[n] = '\0';
            std::string message(buffer);
            std::string clientIP = inet_ntoa(clientAddr.sin_addr);
            int clientPort = ntohs(clientAddr.sin_port);
            
            // Обработка сообщений от стай пчел
            if (message.find("HEARTBEAT:") == 0) {
                // Обрабатываем сигнал активности от стаи
                int swarmId;
                if (sscanf(buffer + 10, "%d", &swarmId) == 1) {
                    std::lock_guard<std::mutex> lock(swarmsMutex);
                    if (beeSwarms.find(swarmId) != beeSwarms.end() && !beeSwarms[swarmId].disconnected) {
                        beeSwarms[swarmId].lastSeen = time(nullptr);
                        
                        // Отправляем ответ для поддержания соединения
                        char response[] = "HEARTBEAT_ACK";
                        sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientAddr, addrLen);
                    }
                }
                
            } else if (message.find("REQUEST:") == 0) {
                // Если Винни-Пух уже найден, отправляем сообщение о завершении поиска
                if (winnieFoundByBees) {
                    char response[] = "WINNIE_FOUND";
                    sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientAddr, addrLen);
                    continue;
                }
                
                // Обрабатываем запрос на поиск сектора
                int swarmId;
                if (sscanf(buffer + 8, "%d", &swarmId) == 1) {
                    bool allowRequest = false;
                    
                    // Проверяем и обновляем информацию о стае
                    {
                        std::lock_guard<std::mutex> lock(swarmsMutex);
                        if (beeSwarms.find(swarmId) == beeSwarms.end()) {
                            // Новая стая
                            beeSwarms[swarmId] = {swarmId, -1, true, false, false, clientIP, clientPort, time(nullptr)};
                            std::cout << "Сервер: Стая #" << swarmId << " подключена" << std::endl;
                            allowRequest = true;
                        } else if (!beeSwarms[swarmId].disconnected) {
                            // Существующая активная стая
                            beeSwarms[swarmId].lastSeen = time(nullptr);
                            beeSwarms[swarmId].ip = clientIP;
                            beeSwarms[swarmId].port = clientPort;
                            
                            // Разрешаем запрос только если стая не выполняет поиск
                            if (!beeSwarms[swarmId].searchInProgress) {
                                beeSwarms[swarmId].active = true;
                                allowRequest = true;
                            }
                        } else if (beeSwarms[swarmId].disconnected) {
                            // Отключенная стая пытается переподключиться
                            if (!beeSwarms[swarmId].active) {
                                beeSwarms[swarmId].disconnected = false;
                                beeSwarms[swarmId].active = true;
                                beeSwarms[swarmId].lastSeen = time(nullptr);
                                beeSwarms[swarmId].ip = clientIP;
                                beeSwarms[swarmId].port = clientPort;
                                beeSwarms[swarmId].searchInProgress = false;
                                beeSwarms[swarmId].currentSector = -1;
                                std::cout << "Сервер: Стая #" << swarmId << " переподключена" << std::endl;
                                allowRequest = true;
                            }
                        }
                    }
                    
                    if (!allowRequest) {
                        // Отказываем в запросе
                        char response[] = "DENIED";
                        sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientAddr, addrLen);
                        continue;
                    }
                    
                    // Поиск неисследованного и неназначенного сектора
                    int sectorToSearch = -1;
                    {
                        std::lock_guard<std::mutex> lock(sectorsMutex);
                        for (auto& sector : sectors) {
                            if (!sector.searched && !sector.assigned) {
                                sectorToSearch = sector.id;
                                sector.assigned = true;  // Помечаем сектор как назначенный
                                break;
                            }
                        }
                    }
                    
                    if (sectorToSearch == -1) {
                        // Нет доступных секторов
                        std::cout << "Сервер: Все доступные секторы назначены" << std::endl;
                        char response[] = "NO_MORE_SECTORS";
                        sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientAddr, addrLen);
                        
                        // Проверяем, все ли секторы исследованы
                        bool allSearched = true;
                        {
                            std::lock_guard<std::mutex> lock(sectorsMutex);
                            for (const auto& sector : sectors) {
                                if (!sector.searched) {
                                    allSearched = false;
                                    break;
                                }
                            }
                        }
                        
                        if (allSearched) {
                            allSectorsSearched = true;
                        }
                    } else {
                        // Отправляем номер сектора для исследования
                        char response[32];
                        sprintf(response, "SEARCH:%d", sectorToSearch);
                        sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientAddr, addrLen);
                        std::cout << "Сервер: Стая #" << swarmId << " направлена в сектор " << sectorToSearch << std::endl;
                        
                        // Обновляем статус стаи
                        std::lock_guard<std::mutex> lock(swarmsMutex);
                        beeSwarms[swarmId].currentSector = sectorToSearch;
                        beeSwarms[swarmId].searchInProgress = true;
                    }
                }
                
            } else if (message.find("REPORT:") == 0) {
                // Обрабатываем отчет о поиске
                int swarmId, sectorId;
                if (sscanf(buffer + 7, "%d:%d:", &swarmId, &sectorId) == 2) {
                    // Обновляем время последней активности
                    {
                        std::lock_guard<std::mutex> lock(swarmsMutex);
                        if (beeSwarms.find(swarmId) != beeSwarms.end() && !beeSwarms[swarmId].disconnected) {
                            beeSwarms[swarmId].lastSeen = time(nullptr);
                            beeSwarms[swarmId].searchInProgress = false;  // Поиск завершен
                            beeSwarms[swarmId].currentSector = -1;        // Стая вернулась в улей
                        } else {
                            // Если стая не зарегистрирована или отключена, игнорируем отчет
                            continue;
                        }
                    }
                    
                    // Проверяем, находится ли Винни-Пух в этом секторе
                    bool isWinnieInSector = (sectorId == winnieSector);
                    
                    // Обновляем статус сектора
                    {
                        std::lock_guard<std::mutex> lock(sectorsMutex);
                        for (auto& sector : sectors) {
                            if (sector.id == sectorId) {
                                sector.searched = true;      // Отмечаем сектор как исследованный
                                sector.assigned = false;     // Сектор больше не назначен
                                if (isWinnieInSector) {
                                    sector.winnieFound = true;
                                }
                                break;
                            }
                        }
                    }
                    
                    if (isWinnieInSector) {
                        std::cout << "Сервер: Стая пчел #" << swarmId << " сообщает, что Винни-Пух найден в секторе " << sectorId << " и наказан!" << std::endl;
                        winnieFoundByBees = true;
                        
                        // Отправляем подтверждение о находке стае, которая нашла Винни-Пуха
                        char response[] = "WINNIE_FOUND";
                        sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientAddr, addrLen);
                        
                        // Уведомляем все другие стаи о находке
                        notifyAllSwarmsWinnieFound();
                    } else {
                        std::cout << "Сервер: Стая пчел #" << swarmId << " сообщает, что сектор " << sectorId << " проверен, Винни-Пух не обнаружен" << std::endl;
                        
                        // Отправляем подтверждение и инструкцию продолжить поиск
                        char response[] = "CONTINUE";
                        sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientAddr, addrLen);
                    }
                }
                
            } else if (message == "DISCONNECT") {
                // Обрабатываем запрос на отключение
                int swarmIdToDisconnect = -1;
                
                // Ищем стаю по IP и порту
                {
                    std::lock_guard<std::mutex> lock(swarmsMutex);
                    for (const auto& [id, swarm] : beeSwarms) {
                        if (swarm.ip == clientIP && swarm.port == clientPort) {
                            swarmIdToDisconnect = id;
                            break;
                        }
                    }
                    
                    if (swarmIdToDisconnect != -1) {
                        std::cout << "Сервер: Стая #" << swarmIdToDisconnect << " запросила отключение" << std::endl;
                        beeSwarms[swarmIdToDisconnect].disconnected = true;
                        beeSwarms[swarmIdToDisconnect].active = false;
                        
                        // Если стая выполняла поиск, освобождаем сектор
                        if (beeSwarms[swarmIdToDisconnect].searchInProgress && beeSwarms[swarmIdToDisconnect].currentSector >= 0) {
                            std::lock_guard<std::mutex> sectorsLock(sectorsMutex);
                            for (auto& sector : sectors) {
                                if (sector.id == beeSwarms[swarmIdToDisconnect].currentSector) {
                                    sector.assigned = false;  // Освобождаем сектор
                                    break;
                                }
                            }
                        }
                        
                        beeSwarms[swarmIdToDisconnect].searchInProgress = false;
                        beeSwarms[swarmIdToDisconnect].currentSector = -1;
                    }
                }
                
                // Отправляем подтверждение отключения
                char response[] = "DISCONNECT_ACK";
                sendto(sockfd, response, strlen(response), 0, (struct sockaddr*)&clientAddr, addrLen);
            }
        }
        
        // Проверяем сообщения от мониторов
        memset(buffer, 0, sizeof(buffer));
        n = recvfrom(monitorSockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&monitorAddr, &monitorAddrLen);
        
        if (n > 0) {
            buffer[n] = '\0';
            std::string message(buffer);
            std::string monitorIP = inet_ntoa(monitorAddr.sin_addr);
            int monitorPort = ntohs(monitorAddr.sin_port);
            
            if (message == "CONNECT_MONITOR") {
                // Регистрируем новый монитор
                {
                    std::lock_guard<std::mutex> lock(monitorsMutex);
                    Monitor newMonitor = {monitorIP, monitorPort, time(nullptr)};
                    monitors.insert(newMonitor);
                }
                
                std::cout << "Сервер: Монитор подключен с " << monitorIP << ":" << monitorPort << std::endl;
                
                // Отправляем начальную информацию
                char response[1024];
                sprintf(response, "INIT:%d:%d", MAX_SECTORS, winnieSector);
                sendto(monitorSockfd, response, strlen(response), 0, (struct sockaddr*)&monitorAddr, monitorAddrLen);
                
            } else if (message == "STATUS") {
                // Обновляем время последнего контакта с монитором
                {
                    std::lock_guard<std::mutex> lock(monitorsMutex);
                    for (auto& monitor : monitors) {
                        if (monitor.ip == monitorIP && monitor.port == monitorPort) {
                            const_cast<Monitor&>(monitor).lastSeen = time(nullptr);
                            break;
                        }
                    }
                }
                
                // Формируем статусное сообщение для монитора
                std::string status = "STATUS:";
                
                // Добавляем информацию о секторах
                {
                    std::lock_guard<std::mutex> lock(sectorsMutex);
                    for (const auto& sector : sectors) {
                        status += std::to_string(sector.id) + ":" + 
                                (sector.searched ? "1:" : "0:") + 
                                (sector.winnieFound ? "1:" : "0:");
                    }
                }
                
                status += "BEES:";
                
                // Добавляем информацию о стаях
                {
                    std::lock_guard<std::mutex> lock(swarmsMutex);
                    for (const auto& [id, swarm] : beeSwarms) {
                        status += std::to_string(swarm.id) + ":" + 
                                std::to_string(swarm.currentSector) + ":" + 
                                (swarm.active ? "1:" : "0:") +
                                (swarm.disconnected ? "1:" : "0:");
                    }
                }
                
                // Добавляем общий статус игры
                status += "GAME:";
                status += winnieFoundByBees ? "1:" : "0:";
                status += allSectorsSearched ? "1" : "0";
                
                sendto(monitorSockfd, status.c_str(), status.length(), 0, 
                      (struct sockaddr*)&monitorAddr, monitorAddrLen);
                
            } else if (message == "DISCONNECT_MONITOR") {
                // Удаляем монитор из списка
                {
                    std::lock_guard<std::mutex> lock(monitorsMutex);
                    for (auto it = monitors.begin(); it != monitors.end(); ) {
                        if (it->ip == monitorIP && it->port == monitorPort) {
                            it = monitors.erase(it);
                            std::cout << "Сервер: Монитор отключен с " << monitorIP << ":" << monitorPort << std::endl;
                        } else {
                            ++it;
                        }
                    }
                }
            }
        }
        
        // Небольшая пауза для уменьшения нагрузки на CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Устанавливаем флаг для завершения потоков
    running = false;
    
    // Ждем завершения потоков
    if (controlThread.joinable()) controlThread.join();
    if (monitThread.joinable()) monitThread.join();

    std::cout << "Сервер: Поиск завершен!" << std::endl;
    if (winnieFoundByBees) {
        std::cout << "Сервер: Винни-Пух был найден и наказан!" << std::endl;
    } else if (allSectorsSearched) {
        std::cout << "Сервер: Все секторы проверены, но Винни-Пух не найден." << std::endl;
    }
    
    // Отправляем сообщение о завершении всем клиентам
    notifyClientsServerShutdown();
    
    // Закрываем сокеты
    close(sockfd);
    close(monitorSockfd);
    std::cout << "Сервер: Работа завершена." << std::endl;
    return 0;
}
