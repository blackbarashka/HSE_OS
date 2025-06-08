// bee_controller_9.cpp
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
#include <cstdlib>
#include <signal.h>
#include <limits>
// ANSI-коды цветов для терминала
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define BOLD          "\033[1m"

// Флаг для обработки Ctrl+C
volatile bool running = true;

// Структура для хранения информации о стае пчел
struct BeeInfo {
    int id;
    bool active;
    bool disconnected;
    int currentSector;
};

// Обработчик сигнала для корректного завершения
void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        std::cout << "\nЗавершение работы контроллера..." << std::endl;
        running = false;
    }
}

// Функция для отправки команды на сервер и получения ответа
std::string sendCommand(int sockfd, const std::string& command, const struct sockaddr_in& serverAddr) {
    // Отправляем команду
    if (sendto(sockfd, command.c_str(), command.length(), 0, 
              (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        return "ERROR:Ошибка отправки команды";
    }
    
    // Получаем ответ
    char buffer[4096] = {0};
    socklen_t addrLen = sizeof(serverAddr);
    
    // Устанавливаем таймаут
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, 
                    (struct sockaddr*)&serverAddr, &addrLen);
    
    if (n < 0) {
        return "ERROR:Таймаут ожидания ответа от сервера";
    }
    
    buffer[n] = '\0';
    return std::string(buffer);
}

// Функция для парсинга списка стай из ответа сервера
std::map<int, BeeInfo> parseBeesInfo(const std::string& response) {
    std::map<int, BeeInfo> bees;
    
    if (response.substr(0, 5) != "BEES:") {
        return bees;
    }
    
    std::string data = response.substr(5);
    std::istringstream ss(data);
    std::string token;
    
    while (std::getline(ss, token, ':')) {
        BeeInfo info;
        info.id = std::stoi(token);
        
        if (std::getline(ss, token, ':')) info.active = (token == "1");
        if (std::getline(ss, token, ':')) info.disconnected = (token == "1");
        if (std::getline(ss, token, ':')) info.currentSector = std::stoi(token);
        
        bees[info.id] = info;
    }
    
    return bees;
}

// Функция для отображения списка стай
void displayBees(const std::map<int, BeeInfo>& bees) {
    std::cout << BOLD << COLOR_CYAN << "=== Список стай пчел ===" << COLOR_RESET << std::endl;
    
    if (bees.empty()) {
        std::cout << COLOR_YELLOW << "Нет активных стай" << COLOR_RESET << std::endl;
        return;
    }
    
    std::cout << BOLD << "ID  | Статус               | Текущий сектор" << COLOR_RESET << std::endl;
    std::cout << "----+----------------------+---------------" << std::endl;
    
    for (const auto& [id, info] : bees) {
        std::string status;
        std::string color;
        
        if (info.disconnected) {
            status = "Отключена";
            color = COLOR_RED;
        } else if (info.active) {
            status = "Активна";
            color = COLOR_GREEN;
        } else {
            status = "Неактивна";
            color = COLOR_YELLOW;
        }
        
        std::string sectorInfo;
        if (info.currentSector >= 0) {
            sectorInfo = std::to_string(info.currentSector);
        } else {
            sectorInfo = "В улье";
        }
        
        printf("%s%-3d%s | %s%-20s%s | %s\n", 
               BOLD, info.id, COLOR_RESET, 
               color.c_str(), status.c_str(), COLOR_RESET, 
               sectorInfo.c_str());
    }
}

// Функция для парсинга статуса из ответа сервера
void parseStatus(const std::string& response) {
    if (response.substr(0, 7) != "STATUS:") {
        std::cout << COLOR_RED << "Ошибка: неверный формат ответа" << COLOR_RESET << std::endl;
        return;
    }
    
    std::string data = response.substr(7);
    std::istringstream ss(data);
    std::string token;
    
    // Парсинг информации о секторах
    if (std::getline(ss, token, ':') && token == "SECTORS") {
        int totalSectors = 0, searchedSectors = 0;
        if (std::getline(ss, token, ':')) totalSectors = std::stoi(token);
        if (std::getline(ss, token, ':')) searchedSectors = std::stoi(token);
        
        std::cout << BOLD << COLOR_CYAN << "Информация о секторах:" << COLOR_RESET << std::endl;
        std::cout << "Всего секторов: " << totalSectors << std::endl;
        std::cout << "Исследовано: " << searchedSectors << " (" 
                  << (totalSectors > 0 ? (searchedSectors * 100 / totalSectors) : 0) << "%)" << std::endl;
    }
    
    // Парсинг информации о стаях
    if (std::getline(ss, token, ':') && token == "BEES") {
        int totalBees = 0, activeBees = 0, disconnectedBees = 0;
        if (std::getline(ss, token, ':')) totalBees = std::stoi(token);
        if (std::getline(ss, token, ':')) activeBees = std::stoi(token);
        if (std::getline(ss, token, ':')) disconnectedBees = std::stoi(token);
        
        std::cout << BOLD << COLOR_CYAN << "Информация о стаях:" << COLOR_RESET << std::endl;
        std::cout << "Всего стай: " << totalBees << std::endl;
        std::cout << "Активные стаи: " << activeBees << std::endl;
        std::cout << "Отключенные стаи: " << disconnectedBees << std::endl;
    }
    
    // Парсинг статуса игры
    if (std::getline(ss, token, ':') && token == "GAME") {
        bool winnieFound = false, allSectorsSearched = false;
        if (std::getline(ss, token, ':')) winnieFound = (token == "1");
        if (std::getline(ss, token, ':')) allSectorsSearched = (token == "1");
        
        std::cout << BOLD << COLOR_CYAN << "Статус поиска:" << COLOR_RESET << std::endl;
        if (winnieFound) {
            std::cout << COLOR_GREEN << "Винни-Пух найден и наказан!" << COLOR_RESET << std::endl;
        } else if (allSectorsSearched) {
            std::cout << COLOR_YELLOW << "Все секторы исследованы, но Винни-Пух не найден" << COLOR_RESET << std::endl;
        } else {
            std::cout << COLOR_BLUE << "Поиск продолжается..." << COLOR_RESET << std::endl;
        }
    }
}

// Функция для очистки экрана
void clearScreen() {
    // Для UNIX/Linux/MacOS
    #if defined(__unix__) || defined(__unix) || defined(__APPLE__)
        system("clear");
    // Для Windows
    #elif defined(_WIN32) || defined(_WIN64)
        system("cls");
    #endif
}

// Функция для отображения меню
void displayMenu() {
    clearScreen();
    std::cout << BOLD << COLOR_CYAN << "=== Управление стаями пчел ===" << COLOR_RESET << std::endl;
    std::cout << COLOR_WHITE << "1. " << COLOR_GREEN << "Показать список стай" << COLOR_RESET << std::endl;
    std::cout << COLOR_WHITE << "2. " << COLOR_YELLOW << "Отключить стаю" << COLOR_RESET << std::endl;
    std::cout << COLOR_WHITE << "3. " << COLOR_GREEN << "Переподключить стаю" << COLOR_RESET << std::endl;
    std::cout << COLOR_WHITE << "4. " << COLOR_BLUE << "Показать общий статус" << COLOR_RESET << std::endl;
    std::cout << COLOR_WHITE << "5. " << COLOR_MAGENTA << "Запустить новые стаи" << COLOR_RESET << std::endl;
    std::cout << COLOR_WHITE << "6. " << COLOR_RED << "Выход" << COLOR_RESET << std::endl;
    std::cout << BOLD << COLOR_WHITE << "Выберите действие (1-6): " << COLOR_RESET;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << COLOR_RED << "Использование: " << argv[0] << " <IP сервера> <CONTROL_PORT>" << COLOR_RESET << std::endl;
        return 1;
    }
    
    // Устанавливаем обработчик сигнала для корректного завершения
    signal(SIGINT, signalHandler);
    
    const char* serverIP = argv[1];
    int controlPort = std::stoi(argv[2]);
    int serverPort = controlPort - 2000;  // Порт для запуска новых стай
    
    // Создаем UDP сокет для связи с сервером
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Ошибка создания сокета");
        return 1;
    }
    
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(controlPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);
    
    std::cout << BOLD << COLOR_GREEN << "Контроллер стай пчел запущен" << COLOR_RESET << std::endl;
    std::cout << "Подключение к серверу: " << serverIP << ":" << controlPort << std::endl;
    
    // Проверяем доступность сервера
    std::string response = sendCommand(sockfd, "STATUS", serverAddr);
    if (response.find("ERROR:") == 0) {
        std::cout << COLOR_RED << "Ошибка подключения к серверу: " << response.substr(6) << COLOR_RESET << std::endl;
        std::cout << "Нажмите Enter для продолжения...";
        std::cin.get();
    } else {
        std::cout << COLOR_GREEN << "Соединение с сервером установлено" << COLOR_RESET << std::endl;
        std::cout << "Нажмите Enter для продолжения...";
        std::cin.get();
    }
    
    while (running) {
        displayMenu();
        int choice;
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Очищаем буфер ввода
        
        switch (choice) {
            case 1: { // Показать список стай
                std::string response = sendCommand(sockfd, "LIST_BEES", serverAddr);
                
                if (response.find("ERROR:") == 0) {
                    std::cout << COLOR_RED << response.substr(6) << COLOR_RESET << std::endl;
                } else {
                    auto bees = parseBeesInfo(response);
                    displayBees(bees);
                }
                
                std::cout << "Нажмите Enter для продолжения...";
                std::cin.get();
                break;
            }
            
            case 2: { // Отключить стаю
                // Сначала получаем список стай
                std::string response = sendCommand(sockfd, "LIST_BEES", serverAddr);
                
                                if (response.find("ERROR:") == 0) {
                    std::cout << COLOR_RED << response.substr(6) << COLOR_RESET << std::endl;
                } else {
                    auto bees = parseBeesInfo(response);
                    displayBees(bees);
                }
                
                // Запрашиваем ID стаи для отключения
                int swarmId;
                std::cout << "Введите ID стаи для отключения (0 - отмена): ";
                std::cin >> swarmId;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                
                if (swarmId > 0) {
                    std::string command = "DISCONNECT_BEE:" + std::to_string(swarmId);
                    response = sendCommand(sockfd, command, serverAddr);
                    
                    if (response.find("ERROR:") == 0) {
                        std::cout << COLOR_RED << response.substr(6) << COLOR_RESET << std::endl;
                    } else if (response.find("OK:") == 0) {
                        std::cout << COLOR_GREEN << response.substr(3) << COLOR_RESET << std::endl;
                    } else {
                        std::cout << "Ответ сервера: " << response << std::endl;
                    }
                }
                
                std::cout << "Нажмите Enter для продолжения...";
                std::cin.get();
                break;
            }
            
            case 3: { // Переподключить стаю
                // Сначала получаем список стай
                std::string response = sendCommand(sockfd, "LIST_BEES", serverAddr);
                
                if (response.find("ERROR:") == 0) {
                    std::cout << COLOR_RED << response.substr(6) << COLOR_RESET << std::endl;
                } else {
                    auto bees = parseBeesInfo(response);
                    
                    // Отображаем только отключенные стаи
                    std::cout << BOLD << COLOR_CYAN << "=== Отключенные стаи ===" << COLOR_RESET << std::endl;
                    bool hasDisconnected = false;
                    
                    for (const auto& [id, info] : bees) {
                        if (info.disconnected) {
                            hasDisconnected = true;
                            std::cout << "ID: " << id << ", Статус: " << (info.active ? "Активна" : "Неактивна") << std::endl;
                        }
                    }
                    
                    if (!hasDisconnected) {
                        std::cout << COLOR_YELLOW << "Нет отключенных стай" << COLOR_RESET << std::endl;
                    } else {
                        // Запрашиваем ID стаи для переподключения
                        int swarmId;
                        std::cout << "Введите ID стаи для переподключения (0 - отмена): ";
                        std::cin >> swarmId;
                        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                        
                        if (swarmId > 0) {
                            std::string command = "RECONNECT_BEE:" + std::to_string(swarmId);
                            response = sendCommand(sockfd, command, serverAddr);
                            
                            if (response.find("ERROR:") == 0) {
                                std::cout << COLOR_RED << response.substr(6) << COLOR_RESET << std::endl;
                            } else if (response.find("OK:") == 0) {
                                std::cout << COLOR_GREEN << response.substr(3) << COLOR_RESET << std::endl;
                                std::cout << "Теперь необходимо запустить стаю с тем же ID: " << std::endl;
                                std::cout << COLOR_BLUE << "./bee_client_9 " << serverIP << " " << serverPort << " " << swarmId << COLOR_RESET << std::endl;
                            } else {
                                std::cout << "Ответ сервера: " << response << std::endl;
                            }
                        }
                    }
                }
                
                std::cout << "Нажмите Enter для продолжения...";
                std::cin.get();
                break;
            }
            
            case 4: { // Показать общий статус
                std::string response = sendCommand(sockfd, "STATUS", serverAddr);
                
                if (response.find("ERROR:") == 0) {
                    std::cout << COLOR_RED << response.substr(6) << COLOR_RESET << std::endl;
                } else {
                    parseStatus(response);
                }
                
                std::cout << "\nНажмите Enter для продолжения...";
                std::cin.get();
                break;
            }
            
            case 5: { // Запустить новые стаи
                int numSwarms;
                std::cout << "Введите количество новых стай для запуска (1-10): ";
                std::cin >> numSwarms;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                
                if (numSwarms <= 0 || numSwarms > 10) {
                    std::cout << COLOR_RED << "Недопустимое количество стай" << COLOR_RESET << std::endl;
                    std::cout << "Нажмите Enter для продолжения...";
                    std::cin.get();
                    break;
                }
                
                // Получаем информацию о существующих стаях, чтобы не создавать дубликаты ID
                std::string response = sendCommand(sockfd, "LIST_BEES", serverAddr);
                std::map<int, BeeInfo> bees;
                
                if (response.find("ERROR:") != 0) {
                    bees = parseBeesInfo(response);
                }
                
                // Находим свободные ID для новых стай
                int nextId = 1;
                std::vector<int> newIds;
                
                while (newIds.size() < numSwarms) {
                    if (bees.find(nextId) == bees.end()) {
                        newIds.push_back(nextId);
                    }
                    nextId++;
                }
                
                std::cout << COLOR_GREEN << "Запуск " << numSwarms << " новых стай..." << COLOR_RESET << std::endl;
                
                // Запуск стай
                for (int swarmId : newIds) {
                    std::cout << "Запуск стаи #" << swarmId << "... ";
                    
                    // Формируем команду для запуска стаи
                    std::string cmd = "./bee_client_10 " + std::string(serverIP) + " " + 
                                     std::to_string(serverPort) + " " + std::to_string(swarmId) + 
                                     " > bee_" + std::to_string(swarmId) + ".log 2>&1 &";
                    
                    int result = system(cmd.c_str());
                    
                    if (result == 0) {
                        std::cout << COLOR_GREEN << "OK" << COLOR_RESET << std::endl;
                    } else {
                        std::cout << COLOR_RED << "Ошибка" << COLOR_RESET << std::endl;
                    }
                }
                
                std::cout << "Нажмите Enter для продолжения...";
                std::cin.get();
                break;
            }
            
            case 6: { // Выход
                running = false;
                break;
            }
            
            default: {
                std::cout << COLOR_RED << "Неверный выбор. Пожалуйста, выберите число от 1 до 6." << COLOR_RESET << std::endl;
                std::cout << "Нажмите Enter для продолжения...";
                std::cin.get();
                break;
            }
        }
    }
    
    close(sockfd);
    std::cout << BOLD << COLOR_GREEN << "Работа контроллера завершена." << COLOR_RESET << std::endl;
    return 0;
}

