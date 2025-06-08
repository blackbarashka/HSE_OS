#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <fcntl.h>
#include <signal.h>

#define HEARTBEAT_INTERVAL 3
#define MIN_SEARCH_TIME 2
#define MAX_SEARCH_TIME 5

volatile bool running = true;
std::atomic<bool> disconnected(false);
std::atomic<bool> searchInProgress(false);
std::atomic<bool> winnieFound(false);
std::atomic<bool> serverShutdown(false);
int sockfd = -1;
struct sockaddr_in serverAddr;
socklen_t addrLen;
std::mutex mtx;
std::condition_variable cv;
int swarmId = -1;

void signalHandler(int signum) {
    running = false;
    cv.notify_all();
}

void heartbeatThread() {
    while (running && !disconnected && !serverShutdown) {
        if (sockfd >= 0) {
            char heartbeatMsg[32];
            sprintf(heartbeatMsg, "HEARTBEAT:%d", swarmId);
            sendto(sockfd, heartbeatMsg, strlen(heartbeatMsg), 0, (struct sockaddr*)&serverAddr, addrLen);
        }
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, std::chrono::seconds(HEARTBEAT_INTERVAL), []{ return !running || disconnected.load() || serverShutdown.load(); });
    }
}

bool searchForWinnieInSector(int sectorId) {
    std::cout << "Стая #" << swarmId << ": Начинаем поиск в секторе " << sectorId << "..." << std::endl;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> searchTimeDist(MIN_SEARCH_TIME, MAX_SEARCH_TIME);
    searchInProgress = true;
    int searchTime = searchTimeDist(gen);
    for (int i = 0; i < searchTime; i++) {
        if (!running || disconnected || winnieFound || serverShutdown) {
            searchInProgress = false;
            return false;
        }
        std::cout << "Стая #" << swarmId << ": Исследуем сектор " << sectorId << "... [" << (i+1) << "/" << searchTime << "]" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "Стая #" << swarmId << ": Поиск в секторе " << sectorId << " завершен." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    searchInProgress = false;
    return true;
}

bool disconnectFromServer() {
    if (sockfd < 0) return false;
    char disconnectMsg[] = "DISCONNECT";
    sendto(sockfd, disconnectMsg, strlen(disconnectMsg), 0, (struct sockaddr*)&serverAddr, addrLen);
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    char buffer[1024] = {0};
    int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&serverAddr, &addrLen);
    if (n > 0 && strcmp(buffer, "DISCONNECT_ACK") == 0) {
        disconnected = true;
        return true;
    }
    return false;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Использование: " << argv[0] << " <SERVER_IP> <SERVER_PORT> <BEE_SWARM_ID>" << std::endl;
        return 1;
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    const char* serverIP = argv[1];
    int serverPort = std::stoi(argv[2]);
    swarmId = std::stoi(argv[3]);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Ошибка создания сокета");
        return 1;
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);
    addrLen = sizeof(serverAddr);

    std::cout << "Стая пчел #" << swarmId << " готова к поиску Винни-Пуха!" << std::endl;
    std::cout << "Подключение к серверу " << serverIP << ":" << serverPort << std::endl;

    std::thread heartbeat(heartbeatThread);

    bool searching = true;
    while (running && searching && !disconnected && !winnieFound && !serverShutdown) {
        char request[32];
        sprintf(request, "REQUEST:%d", swarmId);
        sendto(sockfd, request, strlen(request), 0, (struct sockaddr*)&serverAddr, addrLen);

        char buffer[1024] = {0};
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        int n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&serverAddr, &addrLen);

        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (!running || disconnected || winnieFound || serverShutdown) break;
                continue;
            } else {
                perror("Ошибка приема");
                break;
            }
        }

        if (strncmp(buffer, "SEARCH:", 7) == 0) {
            int sectorId;
            sscanf(buffer + 7, "%d", &sectorId);
            std::cout << "Стая #" << swarmId << ": Отправляемся в сектор " << sectorId << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!searchForWinnieInSector(sectorId)) break;
            char report[32];
            sprintf(report, "REPORT:%d:%d:", swarmId, sectorId);
            sendto(sockfd, report, strlen(report), 0, (struct sockaddr*)&serverAddr, addrLen);

            memset(buffer, 0, sizeof(buffer));
            n = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&serverAddr, &addrLen);

            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                else break;
            }

            if (strcmp(buffer, "WINNIE_FOUND") == 0) {
                std::cout << "Стая #" << swarmId << ": Винни-Пух найден! Завершаем поиск." << std::endl;
                winnieFound = true;
                searching = false;
            } else if (strcmp(buffer, "CONTINUE") == 0) {
                std::cout << "Стая #" << swarmId << ": Сектор " << sectorId << " проверен, Винни-Пух не обнаружен." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } else if (strcmp(buffer, "SERVER_SHUTDOWN") == 0) {
                std::cout << "Стая #" << swarmId << ": Сервер завершает работу." << std::endl;
                serverShutdown = true;
                break;
            }
        } else if (strcmp(buffer, "DENIED") == 0) {
            std::cout << "Стая #" << swarmId << ": Сервер отклонил подключение." << std::endl;
            disconnected = true;
            break;
        } else if (strcmp(buffer, "NO_MORE_SECTORS") == 0) {
            std::cout << "Стая #" << swarmId << ": Все сектора проверены. Возвращаемся в улей." << std::endl;
            searching = false;
        } else if (strcmp(buffer, "WINNIE_FOUND") == 0) {
            std::cout << "Стая #" << swarmId << ": Получена информация, что Винни-Пух найден другой стаей. Завершаем поиск." << std::endl;
            winnieFound = true;
            searching = false;
        } else if (strcmp(buffer, "SERVER_SHUTDOWN") == 0) {
            std::cout << "Стая #" << swarmId << ": Сервер завершает работу." << std::endl;
            serverShutdown = true;
            break;
        }
    }

    if (searchInProgress) {
        int maxWait = 10;
        while (searchInProgress && maxWait-- > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    if (running && !disconnected && !serverShutdown) {
        disconnectFromServer();
    }

    running = false;
    cv.notify_all();
    if (heartbeat.joinable()) heartbeat.join();
    if (sockfd >= 0) close(sockfd);

    std::cout << "Стая #" << swarmId << ": Работа завершена." << std::endl;
    return 0;
}
