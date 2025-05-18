#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <atomic>

// Флаг для отслеживания состояния работы клиента
std::atomic<bool> running(true);

// Глобальные переменные для повторного использования
int swarmId;
const char* serverIp;
int serverPort;

// Обработчик сигнала для корректного завершения клиента
void signalHandler(int signum) {
    std::cout << "\nПолучен сигнал завершения. Стая #" << swarmId << " завершает работу..." << std::endl;
    running = false;
}

// Функция для уведомления сервера об отключении
void notifyServerDisconnect() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;
    
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(serverPort);
    
    if (inet_pton(AF_INET, serverIp, &serv_addr.sin_addr) <= 0) {
        close(sock);
        return;
    }
    
    // Устанавливаем таймаут для соединения
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
    
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return;
    }
    
    std::string disconnectMsg = "DISCONNECT:" + std::to_string(swarmId);
    send(sock, disconnectMsg.c_str(), disconnectMsg.size(), 0);
    
    char buffer[1024] = {0};
    read(sock, buffer, sizeof(buffer));  // Читаем ответ
    
    close(sock);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Использование: " << argv[0] << " <ID стаи> <IP сервера> <PORT сервера>" << std::endl;
        return 1;
    }

    // Инициализируем глобальные переменные
    swarmId = std::stoi(argv[1]);
    serverIp = argv[2];
    serverPort = std::stoi(argv[3]);

    // Регистрируем обработчик сигнала
    signal(SIGINT, signalHandler);

    std::cout << "Стая пчел #" << swarmId << " начинает работу." << std::endl;

    while (running) {
        // Соединение с сервером для запроса участка
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "Ошибка создания сокета" << std::endl;
            return 1;
        }

        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(serverPort);
        
        if (inet_pton(AF_INET, serverIp, &serv_addr.sin_addr) <= 0) {
            std::cerr << "Неверный адрес" << std::endl;
            close(sock);
            return 1;
        }

        // Устанавливаем таймаут для соединения
        struct timeval tv;
        tv.tv_sec = 3;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Ошибка подключения к серверу. Возможно сервер остановлен." << std::endl;
            std::cerr << "Стая #" << swarmId << " завершает работу." << std::endl;
            close(sock);
            running = false;
            break;
        }

        // Запрашиваем новый участок для поиска
        std::string request = "REQUEST_AREA:" + std::to_string(swarmId);
        send(sock, request.c_str(), request.size(), 0);

        char buffer[1024] = {0};
        int valread = read(sock, buffer, sizeof(buffer));
        
        // Если не удалось прочитать ответ, пробуем снова или выходим
        if (valread <= 0) {
            std::cerr << "Ошибка чтения ответа от сервера" << std::endl;
            close(sock);
            sleep(2);
            continue;
        }
        
        std::string response(buffer);
        close(sock);
        
        // Проверяем на специальное сообщение SHUTDOWN от сервера
        if (response == "SHUTDOWN") {
            std::cout << "Стая #" << swarmId << " получила сигнал завершения работы от сервера." << std::endl;
            std::cout << "Стая #" << swarmId << " корректно завершает работу." << std::endl;
            running = false;
            break;
        }
        
        if (response.find("AREA:") == 0) {
            int areaId = std::stoi(response.substr(5));
            std::cout << "Стая #" << swarmId << " получила задание обыскать участок #" << areaId << std::endl;
            
            // Имитация поиска
            std::cout << "Стая #" << swarmId << " обыскивает участок #" << areaId << "..." << std::endl;
            
            // Проверяем флаг работы во время поиска
            for (int i = 0; i < 4 && running; i++) {
                sleep(1); // Имитация времени поиска (разбитая на части)
                std::cout << "." << std::flush;
            }
            std::cout << std::endl;
            
            // Если получен сигнал остановки, выходим
            if (!running) break;
            
            // Соединение с сервером для отправки отчета о поиске
            sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                std::cerr << "Ошибка создания сокета" << std::endl;
                continue;
            }
            
            if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
                std::cerr << "Ошибка подключения при отправке отчета. Возможно сервер остановлен." << std::endl;
                close(sock);
                running = false;
                break;
            }
            
            std::string searchReport = "SEARCH:" + std::to_string(areaId);
            send(sock, searchReport.c_str(), searchReport.size(), 0);
            
            // Получение результата поиска
            memset(buffer, 0, sizeof(buffer));
            valread = read(sock, buffer, sizeof(buffer));
            
            // Проверка на сигнал SHUTDOWN
            if (valread > 0 && std::string(buffer) == "SHUTDOWN") {
                std::cout << "Стая #" << swarmId << " получила сигнал завершения работы от сервера." << std::endl;
                std::cout << "Стая #" << swarmId << " корректно завершает работу." << std::endl;
                close(sock);
                running = false;
                break;
            }
            
            std::string searchResult(buffer);
            close(sock);
            
            if (searchResult.find("FOUND:") == 0) {
                std::cout << "Стая #" << swarmId << " нашла Винни-Пуха на участке #" << areaId << "!" << std::endl;
                std::cout << "Стая #" << swarmId << " провела показательное наказание Винни-Пуха!" << std::endl;
                std::cout << "Стая #" << swarmId << " возвращается в улей." << std::endl;
                break;
            } else if (searchResult.find("NOTFOUND:") == 0) {
                std::cout << "Стая #" << swarmId << " не нашла Винни-Пуха на участке #" << areaId << "." << std::endl;
                std::cout << "Стая #" << swarmId << " возвращается в улей для получения нового задания." << std::endl;
            } else {
                std::cout << "Стая #" << swarmId << " получила неожиданный ответ: " << searchResult << std::endl;
            }
        } else if (response == "WINNIE_FOUND") {
            std::cout << "Стая #" << swarmId << " узнала, что Винни-Пух уже найден и наказан." << std::endl;
            std::cout << "Стая #" << swarmId << " возвращается в улей." << std::endl;
            break;
        } else if (response == "NO_AREAS_LEFT") {
            std::cout << "Стая #" << swarmId << " узнала, что все участки уже обысканы, но Винни-Пух не найден." << std::endl;
            std::cout << "Стая #" << swarmId << " возвращается в улей." << std::endl;
            break;
        } else if (response == "ALL_AREAS_ASSIGNED") {
            std::cout << "Стая #" << swarmId << " узнала, что все участки уже распределены. Ждём и пробуем снова." << std::endl;
            sleep(3);  // Ждём, пока освободится какой-нибудь участок
        } else {
            std::cout << "Стая #" << swarmId << " получила неожиданный ответ: " << response << std::endl;
            sleep(2);
        }
    }

    // Уведомляем сервер о своем отключении, если это не вызвано остановкой сервера
    if (running) {
        notifyServerDisconnect();
    }
    
    std::cout << "Стая #" << swarmId << " завершила работу." << std::endl;
    return 0;
}
