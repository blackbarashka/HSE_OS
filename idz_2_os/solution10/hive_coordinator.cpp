#include <iostream>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <mqueue.h>
#include <semaphore.h>
#include <signal.h>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <string>

// Имена POSIX объектов IPC
#define COORD_QUEUE "/winnie_coordinator_queue"
#define DISPATCH_QUEUE "/winnie_dispatcher_queue"
#define MUTEX_SEM "/winnie_mutex_sem"

// Коды действий в сообщениях
#define ACTION_START 1      // Начать поиск
#define ACTION_EXPLORED 2   // Участок исследован, Винни не найден
#define ACTION_FOUND 3      // Винни-Пух найден
#define ACTION_FINISH 4     // Завершить поиск

// Структура сообщения
struct Message {
    int action;          // Код действия
    int area;            // Номер участка
    int swarm_id;        // ID стаи
    int total_areas;     // Общее количество участков
    int winnie_area;     // Участок, где скрывается Винни-Пух
};

// Глобальные переменные для очистки ресурсов
mqd_t coord_queue = -1;
sem_t* mutex_sem = nullptr;
bool search_in_progress = true;

// Функция для очистки ресурсов
void cleanup() {
    // Закрываем и удаляем очередь сообщений
    if (coord_queue != -1) {
        mq_close(coord_queue);
        mq_unlink(COORD_QUEUE);
    }
    
    // Закрываем и удаляем семафор
    if (mutex_sem) {
        sem_close(mutex_sem);
        sem_unlink(MUTEX_SEM);
    }
    
    std::cout << "Координатор: ресурсы очищены" << std::endl;
}

// Обработчик сигналов для корректного завершения
void signal_handler(int sig) {
    std::cout << "\nКоординатор: получен сигнал завершения. Останавливаем поиски..." << std::endl;
    search_in_progress = false;
    
    // Отправляем сообщение о завершении
    Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.action = ACTION_FINISH;
    
    // Пытаемся отправить сообщение о завершении
    if (coord_queue != -1) {
        mq_send(coord_queue, (char*)&msg, sizeof(msg), 0);
    }
    
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
    
    // Удаляем все существующие IPC объекты перед созданием новых
    mq_unlink(COORD_QUEUE);
    sem_unlink(MUTEX_SEM);
    
    // Создаем атрибуты для очереди сообщений
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;  // Уменьшено для соответствия системным лимитам
    attr.mq_msgsize = sizeof(Message);  // Точный размер сообщения
    attr.mq_curmsgs = 0;
    
    // Создаем очередь сообщений координатора
    coord_queue = mq_open(COORD_QUEUE, O_CREAT | O_RDWR, 0666, &attr);
    if (coord_queue == (mqd_t)-1) {
        perror("mq_open");
        cleanup();
        exit(1);
    }
    
    // Создаем именованный семафор для взаимного исключения
    mutex_sem = sem_open(MUTEX_SEM, O_CREAT | O_EXCL, 0666, 1);
    if (mutex_sem == SEM_FAILED) {
        perror("sem_open");
        cleanup();
        exit(1);
    }
    
    std::cout << "========== ПОИСК ВИННИ-ПУХА ==========" << std::endl;
    std::cout << "Координатор улья запущен!" << std::endl;
    std::cout << "Всего участков леса: " << num_areas << std::endl;
    std::cout << "Винни-Пух спрятался на участке " << winnie_area << std::endl;
    std::cout << "Ожидаем подключения диспетчера..." << std::endl;
    std::cout << "=======================================" << std::endl;
    
    // Отправляем начальную информацию диспетчеру
    Message init_msg;
    memset(&init_msg, 0, sizeof(init_msg));
    init_msg.action = ACTION_START;
    init_msg.total_areas = num_areas;
    init_msg.winnie_area = winnie_area;
    
    // Ждем некоторое время, чтобы диспетчер успел подключиться
    sleep(3);
    
    if (mq_send(coord_queue, (char*)&init_msg, sizeof(init_msg), 0) == -1) {
        perror("mq_send");
        cleanup();
        exit(1);
    }
    
    std::cout << "Координатор: начальная информация отправлена диспетчеру" << std::endl;
    
    // Главный цикл координатора - обрабатываем статусы поисков
    bool winnie_found = false;
    int areas_explored = 0;
    
    Message status_msg;
    
    while (search_in_progress) {
        // Получаем сообщения о статусах поиска с таймаутом
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1; // 1 секунда
        
        ssize_t received = mq_timedreceive(coord_queue, (char*)&status_msg, sizeof(Message), nullptr, &timeout);
        
        if (received > 0) {
            if (status_msg.action == ACTION_EXPLORED) { // Участок исследован
                sem_wait(mutex_sem);
                areas_explored++;
                sem_post(mutex_sem);
                
                std::cout << "Координатор: получено сообщение, что стая #" << status_msg.swarm_id 
                          << " исследовала участок " << status_msg.area << std::endl;
                
                if (areas_explored >= num_areas) {
                    search_in_progress = false;
                }
            }
            else if (status_msg.action == ACTION_FOUND) { // Винни найден
                winnie_found = true;
                search_in_progress = false;
                
                std::cout << "Координатор: получено сообщение, что стая #" << status_msg.swarm_id 
                          << " нашла Винни-Пуха на участке " << status_msg.area << "!" << std::endl;
            }
            else if (status_msg.action == ACTION_FINISH) { // Завершить поиск
                search_in_progress = false;
                std::cout << "Координатор: получен запрос на завершение поиска" << std::endl;
            }
        }
        
        // Если все исследовано или Винни-Пух найден, завершаем поиски
        if (!search_in_progress) {
            // Отправляем сообщение о завершении поисков
            Message end_msg;
            memset(&end_msg, 0, sizeof(end_msg));
            end_msg.action = ACTION_FINISH;
            
            if (mq_send(coord_queue, (char*)&end_msg, sizeof(end_msg), 0) == -1) {
                perror("mq_send");
            }
            
            break;
        }
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
