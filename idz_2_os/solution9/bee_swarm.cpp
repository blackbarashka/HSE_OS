#include <iostream>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <signal.h>
#include <cstdlib>
#include <ctime>
#include <random>

#define MSG_QUEUE_KEY 0x7890
#define MSG_DISPATCHER_KEY 0x9ABC

// Типы сообщений
#define MSG_TYPE_INFO 1     // Информационное сообщение
#define MSG_TYPE_STATUS 2   // Статус поиска
#define MSG_TYPE_REQUEST 3  // Запрос на участок
#define MSG_TYPE_RESPONSE 4 // Ответ на запрос

// Структура сообщения
struct Message {
    long mtype;
    int action;          // 1 - начать поиск, 2 - участок исследован, 3 - Винни найден, 4 - завершить поиск
    int area;            // Номер участка
    int swarm_id;        // ID стаи
    int total_areas;     // Общее количество участков
    int winnie_area;     // Участок где скрывается Винни-Пух
};

// Глобальные переменные
int main_msg_queue_id = -1;
int dispatcher_msg_queue_id = -1;
bool search_in_progress = true;

// Функция для очистки ресурсов
void cleanup() {
    // Ничего не делаем, так как мы не создаем ресурсы, а только используем
}

// Обработчик сигналов
void signal_handler(int sig) {
    std::cout << "\nСтая: получен сигнал завершения." << std::endl;
    search_in_progress = false;
    cleanup();
    exit(0);
}

int main(int argc, char* argv[]) {
    // Устанавливаем обработчик сигналов
    signal(SIGINT, signal_handler);
    
    // Получаем ID стаи из аргументов или генерируем случайный
    int swarm_id = 0;
    if (argc > 1) {
        swarm_id = std::stoi(argv[1]);
    } else {
        swarm_id = getpid() % 1000;
    }
    
    // Инициализируем генератор случайных чисел с уникальным сидом
    std::random_device rd;
    std::mt19937 gen(rd() + swarm_id);
    std::uniform_int_distribution<> search_time_dist(1, 5);
    
    // Подключаемся к основной очереди сообщений
    main_msg_queue_id = msgget(MSG_QUEUE_KEY, 0666);
    if (main_msg_queue_id == -1) {
        perror("msgget");
        std::cerr << "Не удалось подключиться к основной очереди сообщений. Возможно, координатор не запущен." << std::endl;
        exit(1);
    }
    
    // Подключаемся к очереди сообщений диспетчера
    dispatcher_msg_queue_id = msgget(MSG_DISPATCHER_KEY, 0666);
    if (dispatcher_msg_queue_id == -1) {
        perror("msgget");
        std::cerr << "Не удалось подключиться к очереди диспетчера. Возможно, диспетчер не запущен." << std::endl;
        exit(1);
    }
    
    std::cout << "Стая пчел #" << swarm_id << " вылетела из улья." << std::endl;
    
    // Цикл поиска Винни-Пуха
    while (search_in_progress) {
        // Проверяем сообщения о завершении поиска
        Message end_msg;
        if (msgrcv(main_msg_queue_id, &end_msg, sizeof(Message) - sizeof(long), MSG_TYPE_INFO, IPC_NOWAIT) != -1) {
            if (end_msg.action == 4) { // Завершить поиск
                std::cout << "Стая #" << swarm_id << ": получила сообщение о завершении поиска" << std::endl;
                search_in_progress = false;
                break;
            }
        }
        
        // Запрашиваем участок для исследования
        Message request_msg;
        request_msg.mtype = MSG_TYPE_REQUEST;
        request_msg.swarm_id = swarm_id;
        
        if (msgsnd(dispatcher_msg_queue_id, &request_msg, sizeof(Message) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            break;
        }
        
        std::cout << "Стая #" << swarm_id << ": запрашивает участок для исследования" << std::endl;
        
        // Ожидаем ответ от диспетчера
        Message response_msg;
        if (msgrcv(dispatcher_msg_queue_id, &response_msg, sizeof(Message) - sizeof(long), swarm_id, 0) == -1) {
            perror("msgrcv");
            break;
        }
        
        if (response_msg.action == 4) { // Нет больше участков
            std::cout << "Стая #" << swarm_id << ": получила информацию, что больше нет участков для исследования" << std::endl;
            break;
        }
        
        // Исследуем полученный участок
        int area_to_search = response_msg.area;
        int winnie_area = response_msg.winnie_area;
        
        std::cout << "Стая #" << swarm_id << ": исследует участок " << area_to_search << std::endl;
        
        // Имитация поиска
        int search_time = search_time_dist(gen);
        sleep(search_time);
        
        // Подготавливаем сообщение о результате
        Message result_msg;
        result_msg.mtype = MSG_TYPE_STATUS;
        result_msg.swarm_id = swarm_id;
        result_msg.area = area_to_search;
        
        // Проверяем, находится ли Винни-Пух на этом участке
        if (area_to_search == winnie_area) {
            std::cout << "Стая #" << swarm_id << ": НАШЛА Винни-Пуха на участке " << area_to_search << "!" << std::endl;
            std::cout << "Винни-Пух получает наказание от стаи #" << swarm_id << std::endl;
            
            result_msg.action = 3; // Винни-Пух найден
            if (msgsnd(dispatcher_msg_queue_id, &result_msg, sizeof(Message) - sizeof(long), 0) == -1) {
                perror("msgsnd");
            }
            break;
        } else {
            std::cout << "Стая #" << swarm_id << ": НЕ обнаружила Винни-Пуха на участке " << area_to_search << std::endl;
            
            result_msg.action = 2; // Участок исследован, Винни-Пух не найден
            if (msgsnd(dispatcher_msg_queue_id, &result_msg, sizeof(Message) - sizeof(long), 0) == -1) {
                perror("msgsnd");
            }
        }
        
        // Задержка перед возвращением в улей
        sleep(1);
        std::cout << "Стая #" << swarm_id << ": возвращается в улей" << std::endl;
        
        // Небольшая пауза в улье перед новым вылетом
        sleep(1);
    }
    
    std::cout << "Стая #" << swarm_id << ": завершила поиски и вернулась в улей" << std::endl;
    cleanup();
    
    return 0;
}

