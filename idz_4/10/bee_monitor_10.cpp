// bee_monitor_9.cpp
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
#include <mutex>
#include <condition_variable>
#include <cmath>

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

// Структуры данных для мониторинга
struct Sector {
    int id;
    bool searched = false;
    bool winnieFound = false;
};

struct BeeSwarm {
    int id;
    int currentSector = -1;
    bool active = false;
    bool disconnected = false;
};

// Глобальные переменные для потоков
volatile bool running = true;
std::mutex mtx;
std::condition_variable cv;
int monitorId = 0;

// Глобальные переменные для данных
std::vector<Sector> sectors;
std::map<int, BeeSwarm> beeSwarms;
bool winnieFoundByBees = false;
bool allSectorsSearched = false;
int winnieSector = -1;
int totalSectors = 0;

// Обработчик сигналов
void signalHandler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        std::cout << "\nМонитор #" << monitorId << ": Получен сигнал завершения. Отключаемся от сервера..." << std::endl;
        running = false;
        cv.notify_all();
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

// Функция для отображения карты леса и положения стай
void displayForest() {
    clearScreen();
    
    std::cout << BOLD << COLOR_CYAN << "=== Монитор #" << monitorId << " поиска Винни-Пуха ===" << COLOR_RESET << std::endl;
    std::cout << "Лес разделен на " << totalSectors << " секторов" << std::endl;
    
    // Рассчитываем размеры сетки для визуализации леса
    int gridSize = std::ceil(std::sqrt(totalSectors));
    
    // Отображаем состояние секторов в виде сетки
    std::cout << "\n┌";
    for (int i = 0; i < gridSize; i++) {
        std::cout << "───";
        if (i < gridSize - 1) std::cout << "┬";
    }
    std::cout << "┐\n";
    
    for (int row = 0; row < gridSize; row++) {
        std::cout << "│";
        for (int col = 0; col < gridSize; col++) {
            int sectorId = row * gridSize + col;
            if (sectorId < totalSectors) {
                // Ищем сектор в списке
                bool found = false;
                bool searched = false;
                bool winnieFound = false;
                
                for (const auto& sector : sectors) {
                    if (sector.id == sectorId) {
                        found = true;
                        searched = sector.searched;
                        winnieFound = sector.winnieFound;
                        break;
                    }
                }
                
                // Проверяем, находится ли какая-то стая в этом секторе
                std::string beeInfo = "";
                for (const auto& [id, swarm] : beeSwarms) {
                    if (swarm.currentSector == sectorId && swarm.active) {
                        beeInfo = std::to_string(swarm.id);
                        break;
                    }
                }
                
                if (found) {
                    if (winnieFound && searched) {
                        // Винни-Пух найден
                        std::cout << COLOR_RED << " W ";
                    } else if (searched) {
                        // Сектор проверен, но Винни-Пух не найден
                        std::cout << COLOR_YELLOW << " · ";
                    } else if (!beeInfo.empty()) {
                        // В секторе находится стая
                        std::cout << COLOR_GREEN << " " << beeInfo << " ";
                    } else {
                        // Сектор не проверен
                        std::cout << COLOR_BLUE << " " << sectorId << " ";
                    }
                } else {
                    // Информация о секторе отсутствует
                    std::cout << COLOR_WHITE << " ? ";
                }
                std::cout << COLOR_RESET;
            } else {
                std::cout << "   ";
            }
            if (col < gridSize - 1) std::cout << "│";
        }
        std::cout << "│\n";
        
        if (row < gridSize - 1) {
            std::cout << "├";
            for (int i = 0; i < gridSize; i++) {
                std::cout << "───";
                if (i < gridSize - 1) std::cout << "┼";
            }
            std::cout << "┤\n";
        }
    }
    
    std::cout << "└";
    for (int i = 0; i < gridSize; i++) {
        std::cout << "───";
        if (i < gridSize - 1) std::cout << "┴";
    }
    std::cout << "┘\n";
    
    // Отображаем легенду
    std::cout << "\nЛегенда:";
    std::cout << COLOR_RED << " W " << COLOR_RESET << "- Винни-Пух найден, ";
    std::cout << COLOR_YELLOW << " · " << COLOR_RESET << "- Проверенный сектор, ";
    std::cout << COLOR_GREEN << " N " << COLOR_RESET << "- Стая №N, ";
    std::cout << COLOR_BLUE << " N " << COLOR_RESET << "- ID сектора\n";
    
    // Отображаем статус стай
    std::cout << "\n" << BOLD << COLOR_CYAN << "Активные стаи пчел:" << COLOR_RESET << std::endl;
    bool anyActive = false;
    
    for (const auto& [id, swarm] : beeSwarms) {
        if (swarm.active) {
            anyActive = true;
            std::string status = swarm.currentSector >= 0 ? 
                                 "исследует сектор " + std::to_string(swarm.currentSector) : 
                                 "в улье";
            std::cout << "Стая #" << id << " - " << status << std::endl;
        }
    }
    
    if (!anyActive) {
        std::cout << COLOR_YELLOW << "Нет активных стай" << COLOR_RESET << std::endl;
    }
    
    // Отображаем отключенные стаи
    bool anyDisconnected = false;
    for (const auto& [id, swarm] : beeSwarms) {
        if (swarm.disconnected) {
            if (!anyDisconnected) {
                std::cout << "\n" << COLOR_RED << "Отключенные стаи:" << COLOR_RESET << std::endl;
                anyDisconnected = true;
            }
            std::cout << "Стая #" << id << std::endl;
        }
    }
    
    // Отображаем общий статус
    std::cout << "\n" << BOLD << COLOR_CYAN << "Статус поиска:" << COLOR_RESET << " ";
    if (winnieFoundByBees) {
        std::cout << COLOR_GREEN << "Винни-Пух найден и наказан!" << COLOR_RESET << std::endl;
    } else if (allSectorsSearched) {
        std::cout << COLOR_YELLOW << "Все секторы проверены, но Винни-Пух не найден." << COLOR_RESET << std::endl;
    } else {
        std::cout << COLOR_BLUE << "Поиск продолжается..." << COLOR_RESET << std::endl;
    }
    
    // Показываем время обновления
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::cout << "\nОбновлено: " << std::ctime(&now_time);
    std::cout << "Нажмите Ctrl+C для выхода\n";
}

// Функция для парсинга статуса из ответа сервера
void parseStatusResponse(const std::string& response) {
    if (response.substr(0, 7) != "STATUS:") {
        return;
    }
    
    std::string data = response.substr(7);
    std::istringstream ss(data);
    std::string token;
    
    // Очищаем текущие данные
    sectors.clear();
    beeSwarms.clear();
    
    // Парсинг секторов
    while (std::getline(ss, token, ':')) {
        // Проверяем маркер "BEES:" - конец секции секторов
        if (token == "BEES") break;
        
        Sector sector;
        sector.id = std::stoi(token);
        
        if (std::getline(ss, token, ':')) sector.searched = (token == "1");
        if (std::getline(ss, token, ':')) sector.winnieFound = (token == "1");
        
        sectors.push_back(sector);
    }
    
    // Парсинг стай
    while (std::getline(ss, token, ':')) {
        // Проверяем маркер "GAME:" - конец секции стай
        if (token == "GAME") break;
        
        int swarmId = std::stoi(token);
        BeeSwarm swarm;
        swarm.id = swarmId;
        
        if (std::getline(ss, token, ':')) swarm.currentSector = std::stoi(token);
        if (std::getline(ss, token, ':')) swarm.active = (token == "1");
        if (std::getline(ss, token, ':')) swarm.disconnected = (token == "1");
        
        beeSwarms[swarmId] = swarm;
    }
    
    // Парсинг общего статуса
    if (std::getline(ss, token, ':')) winnieFoundByBees = (token == "1");
    if (std::getline(ss, token, ':')) allSectorsSearched = (token == "1");
}

// Функция для периодического обновления данных от сервера
void statusUpdateThread(int sockfd, struct sockaddr_in serverAddr) {
    while (running) {
        // Отправляем запрос статуса
        const char* request = "STATUS";
        sendto(sockfd, request, strlen(request), 0, 
              (struct sockaddr*)&serverAddr, sizeof(serverAddr));
        
        // Получаем ответ от сервера
        char buffer[4096] = {0};
        socklen_t addrLen = sizeof(serverAddr);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, 
                        (struct sockaddr*)&serverAddr, &addrLen);
        
        if (n > 0) {
            buffer[n] = '\0';
            std::string response(buffer);
            
            // Синхронизированный доступ к данным
            std::lock_guard<std::mutex> lock(mtx);
            parseStatusResponse(response);
        }

	if (strcmp(buffer, "SERVER_SHUTDOWN") == 0) {
    	    std::cout << "Монитор: Получен сигнал завершения от сервера" << std::endl;
    	    running = false;
    	    break;
	 }
        
        // Отображаем обновленную информацию
        displayForest();
        
        // Пауза между обновлениями
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(3), []{ return !running; });
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Использование: " << argv[0] << " <SERVER_IP> <MONITOR_PORT> [MONITOR_ID]" << std::endl;
        return 1;
    }

    const char* serverIP = argv[1];
    int monitorPort = std::stoi(argv[2]);
    if (argc == 4) monitorId = std::stoi(argv[3]);

    // Установка обработчика сигнала для корректного выхода
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Создание UDP сокета
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Ошибка создания сокета");
        return 1;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(monitorPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);

    std::cout << "Монитор #" << monitorId << " поиска Винни-Пуха запущен. Подключаемся к серверу..." << std::endl;
    
    // Отправляем запрос на подключение
    const char* connectMsg = "CONNECT_MONITOR";
    sendto(sockfd, connectMsg, strlen(connectMsg), 0, 
          (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    
    // Получаем начальную информацию от сервера
    char buffer[1024] = {0};
    socklen_t addrLen = sizeof(serverAddr);
    
    // Устанавливаем таймаут для первоначального подключения
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, 
                    (struct sockaddr*)&serverAddr, &addrLen);
    
    if (n < 0) {
        std::cerr << "Ошибка подключения к серверу или таймаут соединения" << std::endl;
        std::cerr << "Убедитесь, что сервер запущен на " << serverIP << ":" << monitorPort << std::endl;
        close(sockfd);
        return 1;
    }
    
    buffer[n] = '\0';
    std::string initResponse(buffer);
    
    // Обрабатываем начальную информацию
    if (initResponse.substr(0, 5) == "INIT:") {
        std::istringstream ss(initResponse.substr(5));
        std::string token;
        
        if (std::getline(ss, token, ':')) totalSectors = std::stoi(token);
        if (std::getline(ss, token, ':')) winnieSector = std::stoi(token);
        
        std::cout << "Подключено к серверу. Лес разделен на " << totalSectors << " секторов." << std::endl;
    } else {
        std::cerr << "Неверный формат начальной информации от сервера" << std::endl;
        close(sockfd);
        return 1;
    }
    
    // Запускаем поток для периодического обновления статуса
    std::thread updateThread(statusUpdateThread, sockfd, serverAddr);
    
    // Ожидаем завершения работы (по сигналу от пользователя)
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // Отправляем сообщение об отключении
    const char* disconnectMsg = "DISCONNECT_MONITOR";
    sendto(sockfd, disconnectMsg, strlen(disconnectMsg), 0, 
          (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    
    // Ждем завершения потока обновления
    if (updateThread.joinable()) updateThread.join();
    
    close(sockfd);
    std::cout << "Монитор #" << monitorId << ": Работа завершена." << std::endl;
    return 0;
}
