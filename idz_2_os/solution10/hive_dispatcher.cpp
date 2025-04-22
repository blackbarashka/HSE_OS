#include <iostream>
#include <vector>
#include <queue>
#include <unistd.h>
#include <fcntl.h>
#include <mqueue.h>
#include <semaphore.h>
#include <signal.h>
#include <cstring>
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
#define ACTION_REQUEST 5    // Запрос участка
#define ACTION_ASSIGN 6     // Назначение участка

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
mqd_t dispatch_queue = -1;
sem_t* mutex_sem = nullptr;
bool search_in_progress = true;

// Функция для очистки ресурсов
void cleanup() {
    // Закрываем очереди сообщений
    if (coord_queue != -1) {
        mq_close(coord_queue);
    }
    
    if (dispatch_queue != -1) {
        mq_close(dispatch_queue);
        mq_unlink(DISPATCH_QUEUE);
    }
    
    // Закрываем семафор
    if (mutex_sem) {
        sem_close(mutex_sem);
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
    
    // Подключаемся к очереди сообщений координатора
    coord_queue = mq_open(COORD_QUEUE, O_RDWR);
    if (coord_queue == (mqd_t)-1) {
        perror("mq_open (coord)");
        std::cerr << "Не удалось подключиться к очереди координатора. Возможно, координатор не запущен." << std::endl;
        exit(1);
    }
    
    // Удаляем существующую очередь диспетчера, если есть
    mq_unlink(DISPATCH_QUEUE);
    
    // Создаем атрибуты для очереди сообщений диспетчера
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;  // Уменьшено для соответствия системным лимитам
    attr.mq_msgsize = sizeof(Message);  // Точный размер сообщения
    attr.mq_curmsgs = 0;
    
    // Создаем очередь сообщений диспетчера
    dispatch_queue = mq_open(DISPATCH_QUEUE, O_CREAT | O_RDWR, 0666, &attr);
    if (dispatch_queue == (mqd_t)-1) {
        perror("mq_open (dispatch)");
        mq_close(coord_queue);
        exit(1);
    }
    
    std::cout << "Диспетчер: очередь сообщений создана" << std::endl;
    
    // Ждем, чтобы дать время стаям подключиться
    sleep(2);
    
    // Подключаемся к семафору
    mutex_sem = sem_open(MUTEX_SEM, O_RDWR);
    if (mutex_sem == SEM_FAILED) {
        perror("sem_open");
        std::cerr << "Не удалось подключиться к семафору. Возможно, координатор не запущен." << std::endl;
        mq_close(coord_queue);
        mq_close(dispatch_queue);
        mq_unlink(DISPATCH_QUEUE);
        exit(1);
    }
    
    std::cout << "Диспетчер улья запущен!" << std::endl;
    
    // Ждем начальной информации от координатора
    Message init_msg;
    ssize_t received = mq_receive(coord_queue, (char*)&init_msg, sizeof(Message), nullptr);
    
    if (received <= 0 || init_msg.action != ACTION_START) {
        std::cerr << "Ошибка при получении начальной информации от координатора" << std::endl;
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
        // Проверяем сообщения от координатора без блокировки
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1; // 1 секунда
        
        // Неблокирующее чтение с таймаутом
        Message coord_msg;
        received = mq_timedreceive(coord_queue, (char*)&coord_msg, sizeof(Message), nullptr, &timeout);
        
        if (received > 0) {
            if (coord_msg.action == ACTION_FINISH) { // Завершить поиск
                std::cout << "Диспетчер: получено сообщение о завершении поиска" << std::endl;
                search_in_progress = false;
                
                // Отправляем сообщение о завершении всем стаям
                Message finish_msg;
                memset(&finish_msg, 0, sizeof(finish_msg));
                finish_msg.action = ACTION_FINISH;
                finish_msg.swarm_id = 0; // Сообщение для всех
                
                if (mq_send(dispatch_queue, (char*)&finish_msg, sizeof(finish_msg), 0) == -1) {
                    perror("mq_send (finish)");
                }
                
                break;
            }
        }
        
        // Получаем сообщение от стай
        Message request_msg;
        received = mq_receive(dispatch_queue, (char*)&request_msg, sizeof(Message), nullptr);
        
        if (received > 0) {
            int swarm_id = request_msg.swarm_id;
            
            if (request_msg.action == ACTION_REQUEST) {
                // Формируем ответ
                Message response_msg;
                memset(&response_msg, 0, sizeof(response_msg));
                response_msg.swarm_id = swarm_id; // ID стаи, которой отвечаем
                
                // Если есть доступные участки
                sem_wait(mutex_sem);
                if (!available_areas.empty()) {
                    int next_area = available_areas.front();
                    available_areas.pop();
                    
                    response_msg.action = ACTION_ASSIGN;
                    response_msg.area = next_area;
                    response_msg.winnie_area = winnie_area;
                    
                    std::cout << "Диспетчер: отправляю стае #" << swarm_id << " задание на исследование участка " << next_area << std::endl;
                } else {
                    response_msg.action = ACTION_FINISH;
                    std::cout << "Диспетчер: сообщаю стае #" << swarm_id << " что больше нет участков для исследования" << std::endl;
                }
                sem_post(mutex_sem);
                
                // Отправляем ответ
                if (mq_send(dispatch_queue, (char*)&response_msg, sizeof(response_msg), 0) == -1) {
                    perror("mq_send");
                    std::cout << "Диспетчер: ошибка при отправке ответа стае #" << swarm_id << std::endl;
                } else {
                    std::cout << "Диспетчер: ответ для стаи #" << swarm_id << " отправлен" << std::endl;
                }
            }
            else if (request_msg.action == ACTION_EXPLORED || request_msg.action == ACTION_FOUND) {
                // Перенаправляем сообщение координатору
                if (mq_send(coord_queue, (char*)&request_msg, sizeof(request_msg), 0) == -1) {
                    perror("mq_send");
                }
                
                if (request_msg.action == ACTION_EXPLORED) {
                    std::cout << "Диспетчер: получено сообщение, что стая #" << swarm_id 
                            << " НЕ нашла Винни-Пуха на участке " << request_msg.area << std::endl;
                    
                    sem_wait(mutex_sem);
                    explored[request_msg.area] = true;
                    sem_post(mutex_sem);
                }
                else { // ACTION_FOUND
                    std::cout << "Диспетчер: получено сообщение, что стая #" << swarm_id 
                            << " НАШЛА Винни-Пуха на участке " << request_msg.area << "!" << std::endl;
                    search_in_progress = false;
                }
            }
        }
    }
    
    std::cout << "Диспетчер: завершаю работу" << std::endl;
    cleanup();
    
    return 0;
}
