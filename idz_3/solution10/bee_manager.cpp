#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <map>
#include <cstdlib>

// ANSI-коды цветов для терминала
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"

// Дополнительные стили
#define BOLD          "\033[1m"
#define UNDERLINE     "\033[4m"

struct BeeSwarm {
    int id;
    pid_t pid;
    bool active;
};

std::map<int, BeeSwarm> swarms;
std::string serverIp;
int serverPort;

// Функция для запуска новой стаи пчёл
void startBeeSwarm(int swarmId) {
    pid_t pid = fork();
    
    if (pid < 0) {
        std::cerr << COLOR_RED << "Ошибка при создании процесса" << COLOR_RESET << std::endl;
        return;
    } else if (pid == 0) {
        // Дочерний процесс - запуск клиента
        std::string id = std::to_string(swarmId);
        std::string port = std::to_string(serverPort);
        
        execl("./client", "client", id.c_str(), serverIp.c_str(), port.c_str(), NULL);
        
        // Код ниже выполнится только при ошибке execl
        std::cerr << COLOR_RED << "Ошибка запуска клиента" << COLOR_RESET << std::endl;
        exit(EXIT_FAILURE);
    } else {
        // Родительский процесс - запоминаем PID запущенного клиента
        BeeSwarm swarm;
        swarm.id = swarmId;
        swarm.pid = pid;
        swarm.active = true;
        swarms[swarmId] = swarm;
        
        std::cout << COLOR_GREEN << "Стая #" << swarmId << " запущена (PID: " << pid << ")" << COLOR_RESET << std::endl;
    }
}

// Функция для остановки стаи
void stopBeeSwarm(int swarmId) {
    if (swarms.find(swarmId) != swarms.end() && swarms[swarmId].active) {
        // Отправляем сигнал SIGINT процессу
        kill(swarms[swarmId].pid, SIGINT);
        swarms[swarmId].active = false;
        std::cout << COLOR_YELLOW << "Стае #" << swarmId << " отправлен сигнал завершения" << COLOR_RESET << std::endl;
    } else {
        std::cout << COLOR_RED << "Стая #" << swarmId << " не найдена или уже не активна" << COLOR_RESET << std::endl;
    }
}

// Функция для проверки статуса сервера
bool checkServerStatus() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return false;
    }
    
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serverPort);
    
    if (inet_pton(AF_INET, serverIp.c_str(), &serv_addr.sin_addr) <= 0) {
        close(sock);
        return false;
    }
    
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof timeout);
    
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return false;
    }
    
    // Отправляем простой STATUS запрос серверу
    std::string status_request = "STATUS";
    send(sock, status_request.c_str(), status_request.size(), 0);
    
    char buffer[1024] = {0};
    int valread = read(sock, buffer, sizeof(buffer));
    
    close(sock);
    return valread > 0;
}

// Функция для отображения списка активных стай
void listActiveSwarms() {
    std::cout << "\n" << BOLD << COLOR_CYAN << "=== Активные стаи пчёл ===" << COLOR_RESET << std::endl;
    bool anyActive = false;
    
    for (const auto& pair : swarms) {
        if (pair.second.active) {
            std::cout << COLOR_GREEN << "Стая #" << pair.second.id << " (PID: " << pair.second.pid << ")" << COLOR_RESET << std::endl;
            anyActive = true;
        }
    }
    
    if (!anyActive) {
        std::cout << COLOR_YELLOW << "Нет активных стай" << COLOR_RESET << std::endl;
    }
    
    std::cout << COLOR_CYAN << "=========================" << COLOR_RESET << std::endl;
}

// Функция для отображения интерактивного меню
void displayMenu() {
    std::cout << "\n" << BOLD << COLOR_CYAN << "=== Менеджер стай пчёл ===" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "1. " << COLOR_GREEN << "Запустить новую стаю" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "2. " << COLOR_YELLOW << "Остановить стаю" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "3. " << COLOR_CYAN << "Показать активные стаи" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "4. " << COLOR_MAGENTA << "Проверить статус сервера" << COLOR_RESET << std::endl;
    std::cout << COLOR_BLUE << "5. " << COLOR_RED << "Выход" << COLOR_RESET << std::endl;
    std::cout << BOLD << COLOR_WHITE << "Выберите действие (1-5): " << COLOR_RESET;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << COLOR_RED << "Использование: " << argv[0] << " <IP сервера> <PORT сервера>" << COLOR_RESET << std::endl;
        return 1;
    }
    
    serverIp = argv[1];
    serverPort = std::stoi(argv[2]);
    
    std::cout << BOLD << COLOR_GREEN << "Менеджер стай пчёл запущен. Сервер: " 
              << COLOR_YELLOW << serverIp << ":" << serverPort << COLOR_RESET << std::endl;
    
    // Проверка доступности сервера
    if (!checkServerStatus()) {
        std::cout << BOLD << COLOR_RED << "ВНИМАНИЕ: Сервер недоступен. Убедитесь, что сервер запущен!" << COLOR_RESET << std::endl;
    } else {
        std::cout << COLOR_GREEN << "Сервер доступен и отвечает на запросы." << COLOR_RESET << std::endl;
    }
    
    int nextSwarmId = 1;
    int choice;
    
    while (true) {
        displayMenu();
        std::cin >> choice;
        
        switch (choice) {
            case 1: {
                // Запустить новую стаю
                std::cout << COLOR_GREEN << "Запуск стаи #" << nextSwarmId << COLOR_RESET << std::endl;
                startBeeSwarm(nextSwarmId++);
                break;
            }
            case 2: {
                // Остановить стаю
                int swarmId;
                listActiveSwarms();
                std::cout << COLOR_YELLOW << "Введите ID стаи для остановки (0 - отмена): " << COLOR_RESET;
                std::cin >> swarmId;
                if (swarmId > 0) {
                    stopBeeSwarm(swarmId);
                }
                break;
            }
            case 3: {
                // Показать активные стаи
                listActiveSwarms();
                break;
            }
            case 4: {
                // Проверить статус сервера
                if (checkServerStatus()) {
                    std::cout << BOLD << COLOR_GREEN << "Сервер доступен и отвечает на запросы." << COLOR_RESET << std::endl;
                } else {
                    std::cout << BOLD << COLOR_RED << "ВНИМАНИЕ: Сервер недоступен!" << COLOR_RESET << std::endl;
                }
                break;
            }
            case 5: {
                // Выход
                std::cout << COLOR_YELLOW << "Остановить все активные стаи перед выходом? (y/n): " << COLOR_RESET;
                char confirm;
                std::cin >> confirm;
                
                if (confirm == 'y' || confirm == 'Y') {
                    for (auto& pair : swarms) {
                        if (pair.second.active) {
                            std::cout << COLOR_YELLOW << "Останавливаем стаю #" << pair.second.id << COLOR_RESET << std::endl;
                            stopBeeSwarm(pair.second.id);
                        }
                    }
                    // Даем время на корректное завершение процессов
                    sleep(1);
                }
                
                std::cout << BOLD << COLOR_GREEN << "Завершение работы менеджера стай." << COLOR_RESET << std::endl;
                return 0;
            }
            default:
                std::cout << COLOR_RED << "Неверный выбор. Пожалуйста, выберите число от 1 до 5." << COLOR_RESET << std::endl;
        }
    }
    
    return 0;
}
