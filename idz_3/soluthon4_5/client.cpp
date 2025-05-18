#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Использование: " << argv[0] << " <ID стаи> <IP сервера> <PORT сервера>" << std::endl;
        return 1;
    }

    int swarmId = std::stoi(argv[1]);
    const char* serverIp = argv[2];
    int serverPort = std::stoi(argv[3]);

    std::cout << "Стая пчел #" << swarmId << " начинает работу." << std::endl;

    while (true) {
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

        if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Ошибка подключения" << std::endl;
            sleep(2);
            close(sock);
            continue;
        }

        // Запрашиваем новый участок для поиска
        std::string request = "REQUEST_AREA:" + std::to_string(swarmId);
        send(sock, request.c_str(), request.size(), 0);

        char buffer[1024] = {0};
        int valread = read(sock, buffer, sizeof(buffer));
        std::string response(buffer);
        
        close(sock);
        
        if (response.find("AREA:") == 0) {
            int areaId = std::stoi(response.substr(5));
            std::cout << "Стая #" << swarmId << " получила задание обыскать участок #" << areaId << std::endl;
            
            // Имитация поиска
            std::cout << "Стая #" << swarmId << " обыскивает участок #" << areaId << "..." << std::endl;
            sleep(2);  // Имитация времени поиска
            
            // Соединение с сервером для отправки отчета о поиске
            sock = socket(AF_INET, SOCK_STREAM, 0);
            connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
            
            std::string searchReport = "SEARCH:" + std::to_string(areaId);
            send(sock, searchReport.c_str(), searchReport.size(), 0);
            
            // Получение результата поиска
            memset(buffer, 0, sizeof(buffer));
            valread = read(sock, buffer, sizeof(buffer));
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
            sleep(2);  // Ждём, пока освободится какой-нибудь участок
        } else {
            std::cout << "Стая #" << swarmId << " получила неожиданный ответ: " << response << std::endl;
            sleep(1);
        }
    }

    return 0;
}
