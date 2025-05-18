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
        std::cerr << "Ошибка при создании процесса" << std::endl;
        return;
    } else if (pid == 0) {
        // Дочерний процесс - запуск клиента
        std::string id = std::to_string(swarmId);
        std::string port = std::to_string(serverPort);
        
        execl("./client", "client", id.c_str(), serverIp.c_str(), port.c_str(), NULL);
        
        // Код ниже выполнится только при ошибке execl
        std::cerr << "Ошибка запуска клиента" << std::endl;
        exit(EXIT_FAILURE);
    } else {
        // Родительский процесс - запоминаем PID запущенного клиента
        BeeSwarm swarm;
        swarm.id = swarmId;
        swarm.pid = pid;
        swarm.active = true;
        swarms[swarmId] = swarm;
        
        std::cout << "Стая #" << swarmId << " запущена (PID: " << pid << ")" << std::endl;
    }
}

// Функция для остановки стаи
void stopBeeSwarm(int swarmId) {
    if (swarms.find(swarmId) != swarms.end() && swarms[swarmId].active) {
        // Отправляем сигнал SIGINT процессу
        kill(swarms[swarmId].pid, SIGINT);
        swarms[swarmId].active = false;
        std::cout << "Стае #" << swarmId << " отправлен сигнал завершения" << std::endl;
    } else {
        std::cout << "Стая #" << swarmId << " не найдена или уже не активна" << std::endl;
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
    std::cout << "\n=== Активные стаи пчёл ===" << std::endl;
    bool anyActive = false;
    
    for (const auto& pair : swarms) {
        if (pair.second.active) {
            std::cout << "Стая #" << pair.second.id << " (PID: " << pair.second.pid << ")" << std::endl;
            anyActive = true;
        }
    }
    
    if (!anyActive) {
        std::cout << "Нет активных стай" << std::endl;
    }
    
    std::cout << "=========================" << std::endl;
}

// Функция для отображения интерактивного меню
void displayMenu() {
    std::cout << "\n=== Менеджер стай пчёл ===\n";
    std::cout << "1. Запустить новую стаю\n";
    std::cout << "2. Остановить стаю\n";
    std::cout << "3. Показать активные стаи\n";
    std::cout << "4. Проверить статус сервера\n";
    std::cout << "5. Выход\n";
    std::cout << "Выберите действие (1-5): ";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <IP сервера> <PORT сервера>" << std::endl;
        return 1;
    }
    
    serverIp = argv[1];
    serverPort = std::stoi(argv[2]);
    
    std::cout << "Менеджер стай пчёл запущен. Сервер: " << serverIp << ":" << serverPort << std::endl;
    
    // Проверка доступности сервера
    if (!checkServerStatus()) {
        std::cout << "ВНИМАНИЕ: Сервер недоступен. Убедитесь, что сервер запущен!" << std::endl;
    } else {
        std::cout << "Сервер доступен и отвечает на запросы." << std::endl;
    }
    
    int nextSwarmId = 1;
    int choice;
    
    while (true) {
        displayMenu();
        std::cin >> choice;
        
        switch (choice) {
            case 1: {
                // Запустить новую стаю
                std::cout << "Запуск стаи #" << nextSwarmId << std::endl;
                startBeeSwarm(nextSwarmId++);
                break;
            }
            case 2: {
                // Остановить стаю
                int swarmId;
                listActiveSwarms();
                std::cout << "Введите ID стаи для остановки (0 - отмена): ";
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
                    std::cout << "Сервер доступен и отвечает на запросы." << std::endl;
                } else {
                    std::cout << "ВНИМАНИЕ: Сервер недоступен!" << std::endl;
                }
                break;
            }
            case 5: {
                // Выход
                std::cout << "Остановить все активные стаи перед выходом? (y/n): ";
                char confirm;
                std::cin >> confirm;
                
                if (confirm == 'y' || confirm == 'Y') {
                    for (auto& pair : swarms) {
                        if (pair.second.active) {
                            std::cout << "Останавливаем стаю #" << pair.second.id << std::endl;
                            stopBeeSwarm(pair.second.id);
                        }
                    }
                    // Даем время на корректное завершение процессов
                    sleep(1);
                }
                
                std::cout << "Завершение работы менеджера стай." << std::endl;
                return 0;
            }
            default:
                std::cout << "Неверный выбор. Пожалуйста, выберите число от 1 до 5." << std::endl;
        }
    }
    
    return 0;
}
