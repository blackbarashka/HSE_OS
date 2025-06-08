// bee_server_5.cpp - сервер для задачи о Винни-Пухе (оценка 4-5)
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>
#include <vector>

#define MAX_SECTORS 10 // Максимальное количество секторов леса

// Структура для хранения информации о секторе
struct Sector {
    int id;
    bool searched = false; // Сектор уже проверен
    bool winnieFound = false; // В секторе найден Винни-Пух
};

int main(int argc, char* argv[]) {
    // Проверка аргументов командной строки
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <IP> <PORT>" << std::endl;
        return 1;
    }

    const char* serverIP = argv[1];
    int serverPort = std::stoi(argv[2]);

    // Инициализация секторов леса
    std::vector<Sector> sectors;
    for (int i = 0; i < MAX_SECTORS; i++) {
        sectors.push_back({i, false, false});
    }
    
    // Случайно размещаем Винни-Пуха в одном из секторов
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, MAX_SECTORS - 1);
    int winnieSector = distrib(gen);
    
    std::cout << "Сервер: Винни-Пух находится в секторе " << winnieSector << std::endl;

    // Создание UDP сокета
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Ошибка создания сокета");
        return 1;
    }

    // Настройка адреса сервера
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);
    serverAddr.sin_port = htons(serverPort);

    // Привязка сокета к адресу
    if (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Ошибка привязки сокета");
        close(sockfd);
        return 1;
    }

    std::cout << "Сервер запущен на " << serverIP << ":" << serverPort << std::endl;
    std::cout << "Ожидание стай пчел..." << std::endl;

    // Флаг для определения, был ли найден Винни-Пух
    bool winnieFoundByBees = false;
    // Флаг для определения, все ли секторы проверены
    bool allSectorsSearched = false;

    // Основной цикл сервера
    while (!winnieFoundByBees && !allSectorsSearched) {
        char buffer[1024] = {0};
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        
        // Получаем сообщение от клиента
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, 
                        (struct sockaddr*)&clientAddr, &addrLen);
        
        if (n > 0) {
            buffer[n] = '\0';
            std::string message(buffer);
            std::string clientIP = inet_ntoa(clientAddr.sin_addr);
            int clientPort = ntohs(clientAddr.sin_port);
            
            std::cout << "Сервер: Получено сообщение от " << clientIP << ":" << clientPort << ": " << message << std::endl;
            
            // Обработка запроса на поиск сектора
            if (message.find("REQUEST:") == 0) {
                int swarmId;
                if (sscanf(buffer + 8, "%d", &swarmId) == 1) {
                    std::cout << "Сервер: Стая #" << swarmId << " запросила сектор для поиска" << std::endl;
                    
                    // Поиск неисследованного сектора
                    int sectorToSearch = -1;
                    for (auto& sector : sectors) {
                        if (!sector.searched) {
                            sectorToSearch = sector.id;
                            sector.searched = true; // Помечаем сектор как исследуемый
                            break;
                        }
                    }
                    
                    if (sectorToSearch == -1) {
                        // Нет доступных секторов
                        std::cout << "Сервер: Все секторы назначены" << std::endl;
                        char response[] = "NO_MORE_SECTORS";
                        sendto(sockfd, response, strlen(response), 0, 
                              (struct sockaddr*)&clientAddr, addrLen);
                        
                        // Проверяем, все ли секторы исследованы
                        bool allSearched = true;
                        for (const auto& sector : sectors) {
                            if (!sector.searched) {
                                allSearched = false;
                                break;
                            }
                        }
                        
                        if (allSearched) {
                            allSectorsSearched = true;
                            std::cout << "Сервер: Все секторы проверены" << std::endl;
                        }
                    } else {
                        // Отправляем номер сектора для исследования
                        char response[32];
                        sprintf(response, "SEARCH:%d", sectorToSearch);
                        sendto(sockfd, response, strlen(response), 0, 
                              (struct sockaddr*)&clientAddr, addrLen);
                        std::cout << "Сервер: Стая #" << swarmId << " направлена в сектор " << sectorToSearch << std::endl;
                    }
                }
            } 
            // Обработка отчета о поиске
            else if (message.find("REPORT:") == 0) {
                int swarmId, sectorId;
                if (sscanf(buffer + 7, "%d:%d:", &swarmId, &sectorId) == 2) {
                    // Проверяем, находится ли Винни-Пух в этом секторе
                    bool isWinnieInSector = (sectorId == winnieSector);
                    
                    // Обновляем информацию о секторе
                    for (auto& sector : sectors) {
                        if (sector.id == sectorId) {
                            sector.searched = true;
                            if (isWinnieInSector) {
                                sector.winnieFound = true;
                            }
                            break;
                        }
                    }
                    
                    if (isWinnieInSector) {
                        std::cout << "Сервер: Стая #" << swarmId << " сообщает, что Винни-Пух найден в секторе " 
                                  << sectorId << " и наказан!" << std::endl;
                        winnieFoundByBees = true;
                        
                        // Отправляем подтверждение о находке
                        char response[] = "WINNIE_FOUND";
                        sendto(sockfd, response, strlen(response), 0, 
                              (struct sockaddr*)&clientAddr, addrLen);
                        
                        // Сообщаем всем подключенным клиентам, что Винни-Пух найден
                        // В этой версии не реализовано, т.к. требуется хранение информации о клиентах
                    } else {
                        std::cout << "Сервер: Стая #" << swarmId << " сообщает, что сектор " 
                                  << sectorId << " проверен, Винни-Пух не обнаружен" << std::endl;
                        
                        // Отправляем подтверждение и инструкцию продолжить поиск
                        char response[] = "CONTINUE";
                        sendto(sockfd, response, strlen(response), 0, 
                              (struct sockaddr*)&clientAddr, addrLen);
                    }
                }
            }
            // Обработка запроса на завершение поиска
            else if (strcmp(buffer, "FINISH") == 0) {
                std::cout << "Сервер: Клиент " << clientIP << ":" << clientPort << " завершает работу" << std::endl;
                
                char response[] = "BYE";
                sendto(sockfd, response, strlen(response), 0, 
                      (struct sockaddr*)&clientAddr, addrLen);
            }
        }
    }
    
    // Завершение работы сервера
    std::cout << "Сервер: Поиск завершен!" << std::endl;
    if (winnieFoundByBees) {
        std::cout << "Сервер: Винни-Пух был найден и наказан!" << std::endl;
    } else if (allSectorsSearched) {
        std::cout << "Сервер: Все секторы проверены, но Винни-Пух не найден. Возможно, он спрятался." << std::endl;
    }
    
    close(sockfd);
    std::cout << "Сервер: Работа завершена." << std::endl;
    return 0;
}
