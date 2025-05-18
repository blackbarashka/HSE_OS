#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <random>
#include <thread>
#include <mutex>
#include <algorithm>
#include <ctime>
#include <map>
#include <csignal>
#include <atomic>

// Глобальные переменные для обработки сигналов
std::atomic<bool> serverRunning(true);
std::vector<int> activeClientSockets;
std::mutex activeClientsMutex;

struct ForestArea {
    int id;
    bool isSearched;      // Участок уже обыскан
    bool isAssigned;      // Участок назначен для поиска
    int assignedToSwarm;  // ID стаи, которой назначен участок
    bool containsWinnieThePooh;
};

struct LogEntry {
    std::string timestamp;
    std::string message;
};

struct Observer {
    int socketFd;
    bool active;
};

struct SwarmInfo {
    int id;
    bool active;
    int lastAssignedArea;
    time_t lastSeen;
};

std::vector<LogEntry> systemLog;
std::mutex logMutex;
std::vector<Observer> observers;
std::mutex observersMutex;
std::map<int, SwarmInfo> swarmRegistry;  // Реестр всех стай
std::mutex swarmMutex;
std::mutex forestAreasMutex;
const int SWARM_TIMEOUT = 10;  // Таймаут в секундах для определения неактивных стай
int serverSocket = -1;         // Глобальная переменная для серверного сокета

std::string getCurrentTimestamp() {
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%H:%M:%S", &tstruct);
    return buf;
}

// Обработчик сигналов для корректного завершения работы
void signalHandler(int signum) {
    std::cout << "\nПолучен сигнал завершения (" << signum << "). Начинаю корректное завершение работы..." << std::endl;
    serverRunning = false;
    
    // Отправляем сообщение о завершении всем клиентам
    std::cout << "Отправляю сигнал завершения всем подключенным клиентам..." << std::endl;
    
    {
        std::lock_guard<std::mutex> lock(activeClientsMutex);
        for (int sock : activeClientSockets) {
            // Отправляем специальное сообщение SHUTDOWN для всех активных соединений
            std::string shutdownMsg = "SHUTDOWN";
            send(sock, shutdownMsg.c_str(), shutdownMsg.size(), 0);
        }
    }
    
    // Закрываем серверный сокет, чтобы прервать accept()
    if (serverSocket != -1) {
        std::cout << "Закрываю серверный сокет..." << std::endl;
        close(serverSocket);
        serverSocket = -1;
    }
    
    std::cout << "Сервер завершает работу. Клиенты будут корректно завершены." << std::endl;
}

void addLogEntry(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    LogEntry entry;
    entry.timestamp = getCurrentTimestamp();
    entry.message = message;
    systemLog.push_back(entry);
    
    // Отправляем новую запись всем активным наблюдателям
    std::string logMessage = entry.timestamp + " - " + entry.message;
    
    std::lock_guard<std::mutex> obsLock(observersMutex);
    for (auto& observer : observers) {
        if (observer.active) {
            if (send(observer.socketFd, logMessage.c_str(), logMessage.size(), 0) < 0) {
                std::cout << "Ошибка отправки данных наблюдателю (сокет: " << observer.socketFd << ")" << std::endl;
                close(observer.socketFd);
                observer.active = false;
            }
        }
    }
    
    // Удаляем неактивные наблюдатели
    observers.erase(
        std::remove_if(observers.begin(), observers.end(), 
                      [](const Observer& o) { return !o.active; }),
        observers.end());
}

void handleObserver(int clientSocket) {
    // Добавляем сокет наблюдателя в список активных клиентов
    {
        std::lock_guard<std::mutex> lock(activeClientsMutex);
        activeClientSockets.push_back(clientSocket);
    }
    
    // Отправляем всю историю логов
    {
        std::lock_guard<std::mutex> lock(logMutex);
        for (const auto& entry : systemLog) {
            std::string logMessage = entry.timestamp + " - " + entry.message;
            if (send(clientSocket, logMessage.c_str(), logMessage.size(), 0) < 0) {
                std::cout << "Ошибка при отправке истории логов наблюдателю" << std::endl;
                break;
            }
            usleep(100000); // Небольшая задержка между сообщениями
        }
    }
    
    // Добавляем сокет в список наблюдателей
    {
        std::lock_guard<std::mutex> lock(observersMutex);
        Observer newObserver;
        newObserver.socketFd = clientSocket;
        newObserver.active = true;
        observers.push_back(newObserver);
        std::cout << "Новый наблюдатель подключен (сокет: " << clientSocket << "), всего наблюдателей: " << observers.size() << std::endl;
    }
    
    // Отправляем приветствие
    std::string welcomeMsg = getCurrentTimestamp() + " - Добро пожаловать! Вы подключены к системе как наблюдатель.";
    send(clientSocket, welcomeMsg.c_str(), welcomeMsg.size(), 0);
    
    // Ждем, пока клиент не закроет соединение или сервер не остановится
    char buffer[1024];
    while (serverRunning && recv(clientSocket, buffer, sizeof(buffer), 0) > 0) {
        // Просто ждем, пока соединение не закроется или сервер не остановится
    }
    
    // Отмечаем наблюдателя как неактивного
    {
        std::lock_guard<std::mutex> lock(observersMutex);
        for (auto& observer : observers) {
            if (observer.socketFd == clientSocket) {
                observer.active = false;
                break;
            }
        }
    }
    
    addLogEntry("Наблюдатель отключился");
    std::cout << "Наблюдатель отключен (сокет: " << clientSocket << ")" << std::endl;
    
    // Удаляем сокет из списка активных клиентов
    {
        std::lock_guard<std::mutex> lock(activeClientsMutex);
        activeClientSockets.erase(
            std::remove(activeClientSockets.begin(), activeClientSockets.end(), clientSocket),
            activeClientSockets.end());
    }
    
    close(clientSocket);
}

// Поток для проверки неактивных стай
void monitorInactiveSwarms(std::vector<ForestArea>& forestAreas) {
    while (serverRunning) {
        sleep(3); // Проверяем каждые 3 секунды
        
        if (!serverRunning) break; // Проверяем флаг остановки сервера
        
        time_t now = time(0);
        std::lock_guard<std::mutex> lockSwarm(swarmMutex);
        std::lock_guard<std::mutex> lockForest(forestAreasMutex);
        
        for (auto& swarmPair : swarmRegistry) {
            if (swarmPair.second.active && 
                (now - swarmPair.second.lastSeen) > SWARM_TIMEOUT) {
                
                int swarmId = swarmPair.first;
                swarmPair.second.active = false;
                
                // Освобождаем участок, если он был назначен
                for (auto& area : forestAreas) {
                    if (area.assignedToSwarm == swarmId && !area.isSearched) {
                        area.isAssigned = false;
                        area.assignedToSwarm = -1;
                        addLogEntry("Стая #" + std::to_string(swarmId) + 
                                   " превысила таймаут неактивности. Участок #" + 
                                   std::to_string(area.id) + 
                                   " снова доступен для поиска.");
                    }
                }
            }
        }
    }
    std::cout << "Монитор неактивных стай завершил работу" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <IP> <PORT>" << std::endl;
        return 1;
    }

    // Установка обработчиков сигналов
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Настройка сервера
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        std::cerr << "Ошибка создания сокета" << std::endl;
        return 1;
    }

    // Повторное использование адреса
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Настройка адреса сервера
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(argv[1]);
    serverAddr.sin_port = htons(std::stoi(argv[2]));

    // Привязка сокета к адресу
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Ошибка привязки" << std::endl;
        close(serverSocket);
        return 1;
    }

    // Слушать входящие подключения
    if (listen(serverSocket, 10) < 0) {
        std::cerr << "Ошибка при попытке прослушивания" << std::endl;
        close(serverSocket);
        return 1;
    }

    std::cout << "Сервер запущен на " << argv[1] << ":" << argv[2] << std::endl;
    std::cout << "Для корректного завершения работы нажмите Ctrl+C" << std::endl;
    addLogEntry("Сервер запущен");

    // Инициализация участков леса (10 участков)
    std::vector<ForestArea> forestAreas(10);
    for (int i = 0; i < 10; ++i) {
        forestAreas[i].id = i + 1;
        forestAreas[i].isSearched = false;
        forestAreas[i].isAssigned = false;
        forestAreas[i].assignedToSwarm = -1;
        forestAreas[i].containsWinnieThePooh = false;
    }
    
    // Случайно размещаем Винни-Пуха на одном из участков
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 9);
    int winnieLocation = distrib(gen);
    forestAreas[winnieLocation].containsWinnieThePooh = true;
    
    std::cout << "Винни-Пух находится на участке #" << winnieLocation + 1 << std::endl;
    addLogEntry("Винни-Пух находится на участке #" + std::to_string(winnieLocation + 1));
    
    bool winnieFound = false;

    // Запускаем поток для проверки неактивных стай
    std::thread inactiveSwarmThread(monitorInactiveSwarms, std::ref(forestAreas));
    inactiveSwarmThread.detach();

    // Основной цикл сервера с проверкой флага остановки
    while (serverRunning) {
        // Настраиваем таймаут для select(), чтобы регулярно проверять флаг остановки
        fd_set readfds;
        struct timeval tv;
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);
        tv.tv_sec = 1;  // Проверяем каждую секунду
        tv.tv_usec = 0;
        
        int activity = select(serverSocket + 1, &readfds, NULL, NULL, &tv);
        
        if (activity < 0) {
            if (errno == EINTR) continue; // Продолжаем, если прерывание вызвано сигналом
            perror("select failed");
            break;
        }
        
        // Проверяем флаг остановки сервера
        if (!serverRunning) break;
        
        // Проверяем, есть ли данные на сокете
        if (activity == 0 || !FD_ISSET(serverSocket, &readfds)) continue;
        
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        // Принимаем входящее соединение
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            if (!serverRunning) break; // Если сервер остановлен, прерываем цикл
            std::cerr << "Ошибка при принятии подключения" << std::endl;
            continue;
        }

        // Добавляем клиентский сокет в список активных (для возможности отправки сигнала завершения)
        {
            std::lock_guard<std::mutex> lock(activeClientsMutex);
            activeClientSockets.push_back(clientSocket);
        }

        char buffer[1024] = {0};
        int valread = read(clientSocket, buffer, sizeof(buffer));
        if (valread <= 0) {
            close(clientSocket);
            continue;
        }

        std::string message(buffer);
        
        // Проверяем, если это наблюдатель
        if (message == "OBSERVER") {
            addLogEntry("Подключился новый наблюдатель");
            std::thread observerThread(handleObserver, clientSocket);
            observerThread.detach();
            continue;
        }
        
        std::cout << "Получено сообщение от клиента: " << message << std::endl;
        addLogEntry("Получено сообщение: " + message);

        // Обработка сообщения от клиента (стаи пчел)
        std::string response;
        
        if (message.find("SEARCH:") == 0) {
            int areaId = std::stoi(message.substr(7)) - 1;
            
            std::lock_guard<std::mutex> lock(forestAreasMutex);
            if (areaId >= 0 && areaId < static_cast<int>(forestAreas.size())) {
                // Обновляем статус участка - он уже обыскан
                forestAreas[areaId].isSearched = true;
                forestAreas[areaId].isAssigned = false;
                forestAreas[areaId].assignedToSwarm = -1;
                
                if (forestAreas[areaId].containsWinnieThePooh) {
                    response = "FOUND:" + std::to_string(areaId + 1);
                    winnieFound = true;
                    std::cout << "Винни-Пух найден на участке #" << areaId + 1 << std::endl;
                    addLogEntry("Винни-Пух найден на участке #" + std::to_string(areaId + 1));
                } else {
                    response = "NOTFOUND:" + std::to_string(areaId + 1);
                    std::cout << "Участок #" << areaId + 1 << " обыскан, Винни-Пух не обнаружен" << std::endl;
                    addLogEntry("Участок #" + std::to_string(areaId + 1) + " обыскан, Винни-Пух не обнаружен");
                }
            } else {
                response = "INVALID_AREA";
                std::cout << "Запрошен несуществующий участок" << std::endl;
                addLogEntry("Запрошен несуществующий участок");
            }
        } else if (message.find("REQUEST_AREA:") == 0) {
            // Извлекаем ID стаи
            int swarmId = std::stoi(message.substr(13));
            
            // Обновляем информацию о стае в реестре
            {
                std::lock_guard<std::mutex> lock(swarmMutex);
                // Если стая есть в реестре, обновляем время последнего контакта
                if (swarmRegistry.find(swarmId) != swarmRegistry.end()) {
                    swarmRegistry[swarmId].lastSeen = time(0);
                    swarmRegistry[swarmId].active = true;
                    addLogEntry("Стая #" + std::to_string(swarmId) + " возобновила работу");
                } 
                // Если стая новая, добавляем ее в реестр
                else {
                    SwarmInfo newSwarm;
                    newSwarm.id = swarmId;
                    newSwarm.active = true;
                    newSwarm.lastAssignedArea = -1;
                    newSwarm.lastSeen = time(0);
                    swarmRegistry[swarmId] = newSwarm;
                    addLogEntry("Стая #" + std::to_string(swarmId) + " начала работу");
                }
            }
            
            std::cout << "Стая #" << swarmId << " запрашивает участок" << std::endl;
            addLogEntry("Стая #" + std::to_string(swarmId) + " запрашивает участок");
            
            std::lock_guard<std::mutex> lock(forestAreasMutex);
            if (winnieFound) {
                response = "WINNIE_FOUND";
                std::cout << "Сообщаем стае #" << swarmId << ", что Винни-Пух уже найден" << std::endl;
                addLogEntry("Сообщаем стае #" + std::to_string(swarmId) + ", что Винни-Пух уже найден");
            } else {
                // Клиент запрашивает новый участок для поиска
                bool foundArea = false;
                for (size_t i = 0; i < forestAreas.size(); ++i) {
                    // Проверяем, не обыскан ли участок и не назначен ли другой стае
                    if (!forestAreas[i].isSearched && !forestAreas[i].isAssigned) {
                        forestAreas[i].isAssigned = true;
                        forestAreas[i].assignedToSwarm = swarmId;
                        response = "AREA:" + std::to_string(i + 1);
                        foundArea = true;
                        std::cout << "Стае #" << swarmId << " назначен участок #" << i + 1 << std::endl;
                        addLogEntry("Стае #" + std::to_string(swarmId) + " назначен участок #" + std::to_string(i + 1));
                        
                        // Обновляем последний назначенный участок в реестре стай
                        std::lock_guard<std::mutex> lockSwarm(swarmMutex);
                        swarmRegistry[swarmId].lastAssignedArea = i + 1;
                        break;
                    }
                }
                
                if (!foundArea) {
                    // Проверяем, остались ли неназначенные участки
                    bool allAssigned = true;
                    bool allSearched = true;
                    
                    for (const auto& area : forestAreas) {
                        if (!area.isSearched) {
                            allSearched = false;
                            if (!area.isAssigned) {
                                allAssigned = false;
                                break;
                            }
                        }
                    }
                    
                    if (allSearched) {
                        response = "NO_AREAS_LEFT";
                        std::cout << "Сообщаем стае #" << swarmId << ", что все участки уже обысканы" << std::endl;
                        addLogEntry("Сообщаем стае #" + std::to_string(swarmId) + ", что все участки уже обысканы");
                    } else if (allAssigned) {
                        response = "ALL_AREAS_ASSIGNED";
                        std::cout << "Сообщаем стае #" << swarmId << ", что все участки уже назначены" << std::endl;
                        addLogEntry("Сообщаем стае #" + std::to_string(swarmId) + ", что все участки уже назначены");
                    }
                }
            }
        } else if (message.find("DISCONNECT:") == 0) {
            int swarmId = std::stoi(message.substr(11));
            std::lock_guard<std::mutex> lock(swarmMutex);
            if (swarmRegistry.find(swarmId) != swarmRegistry.end()) {
                swarmRegistry[swarmId].active = false;
                std::cout << "Стая #" << swarmId << " отключилась" << std::endl;
                addLogEntry("Стая #" + std::to_string(swarmId) + " отключилась");
                response = "DISCONNECTED";
                
                // Освобождаем участок, если он был назначен
                std::lock_guard<std::mutex> lockForest(forestAreasMutex);
                for (auto& area : forestAreas) {
                    if (area.assignedToSwarm == swarmId && !area.isSearched) {
                        area.isAssigned = false;
                        area.assignedToSwarm = -1;
                        addLogEntry("Освобожден участок #" + std::to_string(area.id) + 
                                   " после отключения стаи #" + std::to_string(swarmId));
                    }
                }
            } else {
                response = "UNKNOWN_SWARM";
            }
        } else if (message == "STATUS") {
            std::lock_guard<std::mutex> lock(forestAreasMutex);
            if (winnieFound) {
                response = "WINNIE_FOUND";
            } else {
                int leftAreas = 0;
                for (const auto& area : forestAreas) {
                    if (!area.isSearched) leftAreas++;
                }
                response = "ONGOING:" + std::to_string(leftAreas);
                addLogEntry("Запрошен статус: осталось участков - " + std::to_string(leftAreas));
            }
        } else {
            response = "UNKNOWN_COMMAND";
            std::cout << "Получена неизвестная команда от клиента" << std::endl;
            addLogEntry("Получена неизвестная команда от клиента");
        }

        // Отправка ответа клиенту
        send(clientSocket, response.c_str(), response.size(), 0);
        
        // Удаляем клиентский сокет из списка активных
        {
            std::lock_guard<std::mutex> lock(activeClientsMutex);
            activeClientSockets.erase(
                std::remove(activeClientSockets.begin(), activeClientSockets.end(), clientSocket),
                activeClientSockets.end());
        }
        
        close(clientSocket);
    }

    // Ждем завершения всех клиентских соединений
    std::cout << "Ожидание завершения всех клиентских соединений..." << std::endl;
    sleep(2);
    
    // Закрываем все оставшиеся активные клиентские сокеты
    {
        std::lock_guard<std::mutex> lock(activeClientsMutex);
        for (int sock : activeClientSockets) {
            close(sock);
        }
        activeClientSockets.clear();
    }
    
    // Закрываем серверный сокет, если он еще открыт
    if (serverSocket != -1) {
        close(serverSocket);
    }
    
    std::cout << "Сервер успешно завершил работу." << std::endl;
    return 0;
}
