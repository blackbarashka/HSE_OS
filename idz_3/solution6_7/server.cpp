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

std::vector<LogEntry> systemLog;
std::mutex logMutex;
std::vector<int> observerSockets;
std::mutex observerMutex;

std::string getCurrentTimestamp() {
    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%H:%M:%S", &tstruct);
    return buf;
}

void addLogEntry(const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    LogEntry entry;
    entry.timestamp = getCurrentTimestamp();
    entry.message = message;
    systemLog.push_back(entry);
    
    // Отправляем новую запись всем наблюдателям
    std::lock_guard<std::mutex> obsLock(observerMutex);
    for (auto it = observerSockets.begin(); it != observerSockets.end(); ) {
        std::string logMessage = entry.timestamp + " - " + entry.message;
        if (send(*it, logMessage.c_str(), logMessage.size(), 0) < 0) {
            close(*it);
            it = observerSockets.erase(it);
        } else {
            ++it;
        }
    }
}

void handleObserver(int clientSocket) {
    // Отправляем всю историю логов
    {
        std::lock_guard<std::mutex> lock(logMutex);
        for (const auto& entry : systemLog) {
            std::string logMessage = entry.timestamp + " - " + entry.message;
            send(clientSocket, logMessage.c_str(), logMessage.size(), 0);
            usleep(100000); // Небольшая задержка между сообщениями
        }
    }
    
    // Добавляем сокет в список наблюдателей
    {
        std::lock_guard<std::mutex> lock(observerMutex);
        observerSockets.push_back(clientSocket);
    }
    
    // Ждем, пока клиент не закроет соединение
    char buffer[1024];
    while (recv(clientSocket, buffer, sizeof(buffer), 0) > 0) {
        // Просто ждем, пока соединение не закроется
    }
    
    // Удаляем сокет из списка наблюдателей при отключении
    {
        std::lock_guard<std::mutex> lock(observerMutex);
        auto it = std::find(observerSockets.begin(), observerSockets.end(), clientSocket);
        if (it != observerSockets.end()) {
            observerSockets.erase(it);
        }
    }
    close(clientSocket);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <IP> <PORT>" << std::endl;
        return 1;
    }

    // Настройка сервера
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
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
    std::mutex forestAreasMutex; // Мьютекс для синхронизации доступа к участкам

    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        // Принимаем входящее соединение
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            std::cerr << "Ошибка при принятии подключения" << std::endl;
            continue;
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
        close(clientSocket);
    }

    close(serverSocket);
    return 0;
}
