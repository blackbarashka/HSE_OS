#include <iostream>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <signal.h>
#include <cstdlib>
#include <ctime>
#include <string>

#define SEM_KEY 0x5678
#define MSG_QUEUE_KEY 0x7890

// Определение семафора
#define SEM_MUTEX 0

// Типы сообщений
#define MSG_TYPE_INFO 1    // Информационное сообщение
#define MSG_TYPE_STATUS 2  // Статус поиска

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
int msg_queue_id = -1;
bool search_in_progress = true;

// Структура для операций с семафорами System V
union semun {
    int val;
    struct semid_ds* buf;
    unsigned short* array;
};

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

// Функция для очистки ресурсов IPC
void cleanup() {
    // Удаляем семафоры
    if (sem_id != -1) {
        semctl(sem_id, 0, IPC_RMID);
    }
    
    // Удаляем очередь сообщений
    if (msg_queue_id != -1) {
        msgctl(msg_queue_id, IPC_RMID, nullptr);
    }
    
    std::cout << "Ресурсы очищены" << std::endl;
}

// Обработчик сигналов для корректного завершения
void signal_handler(int sig) {
    std::cout << "\nПолучен сигнал завершения. Останавливаем поиски..." << std::endl;
    search_in_progress = false;
    
    // Отправляем сообщение о завершении всем процессам
    Message msg;
    msg.mtype = MSG_TYPE_INFO;
    msg.action = 4; // Завершить поиск
    
    msgsnd(msg_queue_id, &msg, sizeof(Message) - sizeof(long), 0);
    
    sleep(1); // Даем время другим процессам увидеть сообщение
    cleanup();
    exit(0);
}

int main(int argc, char* argv[]) {
    // Устанавливаем обработчик сигналов
    signal(SIGINT, signal_handler);
    
    // Инициализируем генератор случайных чисел
    srand(time(nullptr));
    
    // Определяем параметры поиска
    int num_areas = 20; // По умолчанию 20 участков леса
    
    // Проверяем наличие аргументов командной строки
    if (argc > 1) {
        num_areas = std::stoi(argv[1]);
    }
    
    // Проверяем корректность параметров
    if (num_areas <= 0) num_areas = 20;
    
    // Случайно выбираем участок, где спрятался Винни-Пух
    int winnie_area = rand() % num_areas;
    
    // Создаем набор семафоров
    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("semget");
        exit(1);
    }
    
    // Инициализируем семафор для взаимного исключения
    union semun arg;
    arg.val = 1; // Инициализируем mutex значением 1
    if (semctl(sem_id, SEM_MUTEX, SETVAL, arg) == -1) {
        perror("semctl");
        cleanup();
        exit(1);
    }
    
    // Создаем очередь сообщений
    msg_queue_id = msgget(MSG_QUEUE_KEY, IPC_CREAT | 0666);
    if (msg_queue_id == -1) {
        perror("msgget");
        cleanup();
        exit(1);
    }
    
    std::cout << "========== ПОИСК ВИННИ-ПУХА ==========" << std::endl;
    std::cout << "Координатор улья запущен!" << std::endl;
    std::cout << "Всего участков леса: " << num_areas << std::endl;
    std::cout << "Винни-Пух спрятался на участке " << winnie_area << std::endl;
    std::cout << "=======================================" << std::endl;
    
    // Отправляем начальную информацию диспетчеру
    Message init_msg;
    init_msg.mtype = MSG_TYPE_INFO;
    init_msg.action = 1; // Начать поиск
    init_msg.total_areas = num_areas;
    init_msg.winnie_area = winnie_area;
    
    if (msgsnd(msg_queue_id, &init_msg, sizeof(Message) - sizeof(long), 0) == -1) {
        perror("msgsnd");
        cleanup();
        exit(1);
    }
    
    // Главный цикл координатора - обрабатываем статусы поисков
    bool winnie_found = false;
    int areas_explored = 0;
    
    Message status_msg;
    
    while (search_in_progress) {
        // Получаем сообщения о статусах поиска
        if (msgrcv(msg_queue_id, &status_msg, sizeof(Message) - sizeof(long), MSG_TYPE_STATUS, IPC_NOWAIT) != -1) {
            if (status_msg.action == 2) { // Участок исследован
                sem_operation(sem_id, SEM_MUTEX, -1);
                areas_explored++;
                sem_operation(sem_id, SEM_MUTEX, 1);
                
                std::cout << "Координатор: получено сообщение, что стая #" << status_msg.swarm_id 
                          << " исследовала участок " << status_msg.area << std::endl;
                
                if (areas_explored >= num_areas) {
                    search_in_progress = false;
                }
            }
            else if (status_msg.action == 3) { // Винни найден
                winnie_found = true;
                search_in_progress = false;
                
                std::cout << "Координатор: получено сообщение, что стая #" << status_msg.swarm_id 
                          << " нашла Винни-Пуха на участке " << status_msg.area << "!" << std::endl;
            }
        }
        
        // Если все исследовали или Винни-Пух найден, завершаем поиски
        if (!search_in_progress) {
            // Отправляем сообщение о завершении поисков
            Message end_msg;
            end_msg.mtype = MSG_TYPE_INFO;
            end_msg.action = 4; // Завершить поиск
            
            if (msgsnd(msg_queue_id, &end_msg, sizeof(Message) - sizeof(long), 0) == -1) {
                perror("msgsnd");
            }
            
            break;
        }
        
        // Пауза для снижения нагрузки на процессор
        usleep(100000); // 100ms
    }
    
    // Ждем некоторое время, чтобы все процессы успели получить сообщение о завершении
    sleep(2);
    
    // Выводим итоговую информацию
    std::cout << "\n========== РЕЗУЛЬТАТЫ ПОИСКА ==========" << std::endl;
    if (winnie_found) {
        std::cout << "Поиски завершены! Винни-Пух найден на участке " << winnie_area << " и наказан!" << std::endl;
    } else {
        std::cout << "Поиски завершены! Все " << areas_explored << " участков исследованы." << std::endl;
        std::cout << "Винни-Пуха не нашли, хотя он был на участке " << winnie_area << "." << std::endl;
    }
    std::cout << "=======================================" << std::endl;
    
    // Очищаем ресурсы
    cleanup();
    
    return 0;
}

