#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iomanip>
#include <sstream>

// Цвета для выделения важных сообщений
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"

void printSeparator() {
    std::cout << "----------------------------------------" << std::endl;
}

void printFormattedMessage(const std::string& message) {
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
    
    SearchStats() : totalAreas(10), searchedAreas(0), winnieFound(false), activeSwarms(0) {}
    
    void update(const std::string& message) {
        if (message.find("обыскан") != std::string::npos) {
            searchedAreas++;
        }
        if (message.find("Винни-Пух найден") != std::string::npos) {
            winnieFound = true;
        }
        if (message.find("Стая #") != std::string::npos && message.find("запрашивает участок") != std::string::npos) {
            activeSwarms++;
        }
    }
    
    void display() {
        printSeparator();
        std::cout << "Статистика поиска:" << std::endl;
        std::cout << "- Участков обыскано: " << searchedAreas << "/" << totalAreas << std::endl;
        std::cout << "- Винни-Пух " << (winnieFound ? "найден!" : "ещё не найден") << std::endl;
        std::cout << "- Активных стай: ~" << activeSwarms << std::endl;
        printSeparator();
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <IP сервера> <PORT сервера>" << std::endl;
        return 1;
    }

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

    // Получение и отображение информации от сервера
    char buffer[1024] = {0};
    while (true) {
        int valread = read(sock, buffer, sizeof(buffer) - 1);
        if (valread <= 0) {
            std::cout << "Соединение с сервером разорвано." << std::endl;
            break;
        }
        buffer[valread] = '\0';
        
        std::string fullMessage(buffer);
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

    close(sock);
    return 0;
}
