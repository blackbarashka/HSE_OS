#include <iostream>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <signal.h>
#include <cstdlib>
#include <queue>
#include <vector>

#define SEM_KEY 0x5678
#define MSG_QUEUE_KEY 0x7890
#define MSG_DISPATCHER_KEY 0x9ABC

// Определение семафора
#define SEM_MUTEX 0

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

// Глобальные переменные для очистки ресурсов
int sem_id = -1;
int main_msg_queue_id = -1;
int dispatcher_msg_queue_id = -1;
bool search_in_progress = true;

// Функция для выполнения операции с семафором
void sem_operation(int sem_id, int sem_num, int op) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = op;
    sb.sem_flg = 0;
    
    if (semop(sem_id, &sb, 1) == -1) {
        perror("semop");
        exit(1);
    }
}

// Функция для очистки ресурсов
void cleanup() {
    if (dispatcher_msg_queue_id != -1) {
        msgctl(dispatcher_msg_queue_id, IPC_RMID, nullptr);
    }
    
    std::cout << "Диспетчер: ресурсы очищены" << std::endl;
}

// Обработчик сигналов
void signal_handler(int sig) {
    std::cout << "\nДиспетчер: получен сигнал завершения." << std::endl;
    search_in_progress = false;
    cleanup();
    exit(0);
}

int main() {
    // Устанавливаем обработчик сигналов
    signal(SIGINT, signal_handler);
    
    // Подключаемся к семафору
    sem_id = semget(SEM_KEY, 1, 0666);
    if (sem_id == -1) {
        perror("semget");
        std::cerr << "Не удалось подключиться к семафору. Возможно, координатор не запущен." << std::endl;
        exit(1);
    }
    
    // Подключаемся к основной очереди сообщений
    main_msg_queue_id = msgget(MSG_QUEUE_KEY, 0666);
    if (main_msg_queue_id == -1) {
        perror("msgget");
        std::cerr << "Не удалось подключиться к основной очереди сообщений. Возможно, координатор не запущен." << std::endl;
        exit(1);
    }
    
    // Создаем собственную очередь сообщений
    dispatcher_msg_queue_id = msgget(MSG_DISPATCHER_KEY, IPC_CREAT | 0666);
    if (dispatcher_msg_queue_id == -1) {
        perror("msgget");
        exit(1);
    }
    
    std::cout << "Диспетчер улья запущен!" << std::endl;
    
    // Ждем начальной информации от координатора
    Message init_msg;
    if (msgrcv(main_msg_queue_id, &init_msg, sizeof(Message) - sizeof(long), MSG_TYPE_INFO, 0) == -1) {
        perror("msgrcv");
        cleanup();
        exit(1);
    }
    
    if (init_msg.action != 1) {
        std::cerr << "Ожидалось сообщение о начале поиска. Получено что-то другое." << std::endl;
        cleanup();
        exit(1);
    }
    
    int total_areas = init_msg.total_areas;
    int winnie_area = init_msg.winnie_area;
    
    std::cout << "Диспетчер: получена информация о поиске. Всего участков: " << total_areas << std::endl;
    
    // Инициализация доступных участков
    std::queue<int> available_areas;
    for (int i = 0; i < total_areas; i++) {
        available_areas.push(i);
    }
    
    // Отслеживаем исследованные участки
    std::vector<bool> explored(total_areas, false);
    
    // Основной цикл диспетчера
    while (search_in_progress) {
        // Проверяем сообщения от координатора
        Message coord_msg;
        if (msgrcv(main_msg_queue_id, &coord_msg, sizeof(Message) - sizeof(long), MSG_TYPE_INFO, IPC_NOWAIT) != -1) {
            if (coord_msg.action == 4) { // Завершить поиск
                std::cout << "Диспетчер: получено сообщение о завершении поиска" << std::endl;
                search_in_progress = false;
                break;
            }
        }
        
        // Проверяем запросы от стай пчел
        Message request_msg;
        if (msgrcv(dispatcher_msg_queue_id, &request_msg, sizeof(Message) - sizeof(long), MSG_TYPE_REQUEST, IPC_NOWAIT) != -1) {
            int swarm_id = request_msg.swarm_id;
            
            // Формируем ответ
            Message response_msg;
            response_msg.mtype = swarm_id; // Отправляем конкретной стае
            
            // Если есть доступные участки
            sem_operation(sem_id, SEM_MUTEX, -1);
            if (!available_areas.empty()) {
                int next_area = available_areas.front();
                available_areas.pop();
                
                response_msg.action = 1; // Есть участок
                response_msg.area = next_area;
                response_msg.winnie_area = winnie_area;
                
                std::cout << "Диспетчер: отправляю стае #" << swarm_id << " задание на исследование участка " << next_area << std::endl;
            } else {
                response_msg.action = 4; // Нет участков, завершить поиск
                std::cout << "Диспетчер: сообщаю стае #" << swarm_id << " что больше нет участков для исследования" << std::endl;
            }
            sem_operation(sem_id, SEM_MUTEX, 1);
            
            // Отправляем ответ
            if (msgsnd(dispatcher_msg_queue_id, &response_msg, sizeof(Message) - sizeof(long), 0) == -1) {
                perror("msgsnd");
            }
        }
        
        // Проверяем результаты исследования от стай
        Message result_msg;
        if (msgrcv(dispatcher_msg_queue_id, &result_msg, sizeof(Message) - sizeof(long), MSG_TYPE_STATUS, IPC_NOWAIT) != -1) {
            int swarm_id = result_msg.swarm_id;
            int area = result_msg.area;
            
            if (result_msg.action == 2) { // Участок исследован, Винни-Пух не найден
                std::cout << "Диспетчер: получено сообщение, что стая #" << swarm_id 
                          << " НЕ нашла Винни-Пуха на участке " << area << std::endl;
                
                // Отмечаем участок как исследованный
                sem_operation(sem_id, SEM_MUTEX, -1);
                explored[area] = true;
                sem_operation(sem_id, SEM_MUTEX, 1);
                
                // Перенаправляем сообщение координатору
                if (msgsnd(main_msg_queue_id, &result_msg, sizeof(Message) - sizeof(long), 0) == -1) {
                    perror("msgsnd");
                }
            }
            else if (result_msg.action == 3) { // Винни-Пух найден
                std::cout << "Диспетчер: получено сообщение, что стая #" << swarm_id 
                          << " НАШЛА Винни-Пуха на участке " << area << "!" << std::endl;
                
                // Перенаправляем сообщение координатору
                if (msgsnd(main_msg_queue_id, &result_msg, sizeof(Message) - sizeof(long), 0) == -1) {
                    perror("msgsnd");
                }
                
                search_in_progress = false;
            }
        }
        
        // Пауза для снижения нагрузки на процессор
        usleep(100000); // 100ms
    }
    
    std::cout << "Диспетчер: завершаю работу" << std::endl;
    cleanup();
    
    return 0;
}

