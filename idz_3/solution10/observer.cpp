#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iomanip>
#include <sstream>
#include <signal.h>
#include <atomic>

// Цвета для выделения важных сообщений
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"

// Флаг для отслеживания состояния работы наблюдателя
std::atomic<bool> running(true);

void signalHandler(int signum) {
    std::cout << "\nПолучен сигнал завершения. Наблюдатель завершает работу..." << std::endl;
    running = false;
}

void printSeparator() {
    std::cout << "----------------------------------------" << std::endl;
}

void printFormattedMessage(const std::string& message) {
    // Проверяем на сообщение о завершении работы сервера
    if (message == "SHUTDOWN") {
        std::cout << COLOR_RED << "ВНИМАНИЕ: Получен сигнал завершения от сервера!" << COLOR_RESET << std::endl;
        std::cout << COLOR_RED << "Наблюдатель корректно завершает работу..." << COLOR_RESET << std::endl;
        running = false;
        return;
    }
    
    // Выделяем важные события цветом
    if (message.find("Винни-Пух найден") != std::string::npos) {
        printSeparator();
        std::cout << COLOR_RED << "!!! " << message << " !!!" << COLOR_RESET << std::endl;
        printSeparator();
    }
    else if (message.find("Винни-Пух находится") != std::string::npos) {
        std::cout << COLOR_MAGENTA << message << COLOR_RESET << std::endl;
    }
    else if (message.find("назначен участок") != std::string::npos) {
        std::cout << COLOR_GREEN << message << COLOR_RESET << std::endl;
    }
    else if (message.find("обыскан") != std::string::npos) {
        std::cout << COLOR_BLUE << message << COLOR_RESET << std::endl;
    }
    else {
        std::cout << message << std::endl;
    }
}

// Статистика поиска
struct SearchStats {
    int totalAreas;
    int searchedAreas;
    bool winnieFound;
    int activeSwarms;
    int totalSwarms;
    
    SearchStats() : totalAreas(10), searchedAreas(0), winnieFound(false), activeSwarms(0), totalSwarms(0) {}
    
    void update(const std::string& message) {
        if (message.find("обыскан") != std::string::npos) {
            searchedAreas++;
        }
        if (message.find("Винни-Пух найден") != std::string::npos) {
            winnieFound = true;
        }
        if (message.find("начала работу") != std::string::npos) {
            activeSwarms++;
            totalSwarms++;
        }
        if (message.find("отключилась") != std::string::npos) {
            if (activeSwarms > 0) activeSwarms--;
        }
        if (message.find("возобновила работу") != std::string::npos) {
            activeSwarms++;
        }
    }
    
    void display() {
        printSeparator();
        std::cout << "Статистика поиска:" << std::endl;
        std::cout << "- Участков обыскано: " << searchedAreas << "/" << totalAreas << std::endl;
        std::cout << "- Винни-Пух " << (winnieFound ? "найден!" : "ещё не найден") << std::endl;
        std::cout << "- Активные стаи: " << activeSwarms << " (всего было: " << totalSwarms << ")" << std::endl;
        printSeparator();
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <IP сервера> <PORT сервера>" << std::endl;
        return 1;
    }

    // Регистрируем обработчик сигнала
    signal(SIGINT, signalHandler);

    const char* serverIp = argv[1];
    int serverPort = std::stoi(argv[2]);
    SearchStats stats;

    // Создание сокета
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Ошибка создания сокета" << std::endl;
        return 1;
    }

    // Настройка адреса сервера
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serverPort);
    
    if (inet_pton(AF_INET, serverIp, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Неверный адрес" << std::endl;
        return 1;
    }

    // Подключение к серверу
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Ошибка подключения" << std::endl;
        return 1;
    }

    printSeparator();
    std::cout << COLOR_YELLOW << "=== Наблюдатель за поиском Винни-Пуха ===" << COLOR_RESET << std::endl;
    std::cout << "Подключение к серверу: " << serverIp << ":" << serverPort << std::endl;
    printSeparator();

    // Отправка идентификатора OBSERVER
    std::string message = "OBSERVER";
    send(sock, message.c_str(), message.size(), 0);

    // Настройка для неблокирующего чтения с таймаутом
    fd_set readfds;
    struct timeval tv;

    // Получение и отображение информации от сервера
    char buffer[1024] = {0};
    while (running) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        
        // Задаем таймаут в 0.5 секунды
        tv.tv_sec = 0;
        tv.tv_usec = 500000;
        
        int activity = select(sock + 1, &readfds, NULL, NULL, &tv);
        
        if (activity < 0) {
            // Ошибка выполнения select()
            if (errno == EINTR) continue; // Прерывание системного вызова (например, сигналом)
            std::cerr << "Ошибка select()" << std::endl;
            break;
        }
        
        if (activity == 0) continue; // Таймаут, продолжаем проверку флага running
        
        if (FD_ISSET(sock, &readfds)) {
            int valread = read(sock, buffer, sizeof(buffer) - 1);
            if (valread <= 0) {
                std::cout << "Соединение с сервером разорвано. Сервер, вероятно, остановлен." << std::endl;
                break;
            }
            
            buffer[valread] = '\0';
            std::string fullMessage(buffer);
            
            // Проверяем на специальное сообщение
            if (fullMessage == "SHUTDOWN") {
                printFormattedMessage("SHUTDOWN");
                break;
            }
            
            printFormattedMessage(fullMessage);
            stats.update(fullMessage);
            
            // Обновляем статистику после каждого 5-го сообщения
            static int msgCount = 0;
            msgCount++;
            if (msgCount % 5 == 0) {
                stats.display();
            }
            
            // Если нашли Винни-Пуха, выводим специальное сообщение
            if (fullMessage.find("Винни-Пух найден") != std::string::npos) {
                std::cout << COLOR_RED << "ВАЖНО! Винни-Пух был обнаружен и наказан!" << COLOR_RESET << std::endl;
                stats.display();
            }
        }
    }

    close(sock);
    std::cout << "Наблюдатель завершил работу." << std::endl;
    return 0;
}
