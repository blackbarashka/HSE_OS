#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <mqueue.h>
#include <semaphore.h>
#include <signal.h>
#include <cstring>
#include <ctime>
#include <random>

// Имена POSIX объектов IPC
#define COORD_QUEUE "/winnie_coordinator_queue"
#define DISPATCH_QUEUE "/winnie_dispatcher_queue"

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
bool search_in_progress = true;

// Функция для очистки ресурсов
void cleanup() {
    if (coord_queue != -1) {
        mq_close(coord_queue);
    }
    if (dispatch_queue != -1) {
        mq_close(dispatch_queue);
    }
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
    
    // Подключаемся к очереди сообщений координатора
    coord_queue = mq_open(COORD_QUEUE, O_RDWR);
    if (coord_queue == (mqd_t)-1) {
        perror("mq_open (coord)");
        std::cerr << "Не удалось подключиться к очереди координатора. Возможно, координатор не запущен." << std::endl;
        exit(1);
    }
    
    // Подключаемся к очереди диспетчера с повторными попытками
    int retry_count = 0;
    const int max_retries = 5;
    
    while (retry_count < max_retries) {
        dispatch_queue = mq_open(DISPATCH_QUEUE, O_RDWR);
        if (dispatch_queue != (mqd_t)-1) {
            break;  // Успешное подключение
        }
        
        retry_count++;
        std::cout << "Стая #" << swarm_id << ": попытка #" << retry_count 
                  << " подключиться к очереди диспетчера..." << std::endl;
        sleep(1);  // Ждем перед повторной попыткой
    }
    
    if (dispatch_queue == (mqd_t)-1) {
        perror("mq_open (dispatch)");
        std::cerr << "Не удалось подключиться к очереди диспетчера. Возможно, диспетчер не запущен." << std::endl;
        mq_close(coord_queue);
        exit(1);
    }
    
    // Выводим информацию об очереди для отладки
    struct mq_attr attr;
    if (mq_getattr(dispatch_queue, &attr) != -1) {
        std::cout << "Стая #" << swarm_id << ": информация об очереди: maxmsg=" << attr.mq_maxmsg 
                  << ", msgsize=" << attr.mq_msgsize 
                  << ", curmsgs=" << attr.mq_curmsgs << std::endl;
    }
    
    std::cout << "Стая пчел #" << swarm_id << " вылетела из улья." << std::endl;
    
    // Цикл поиска Винни-Пуха
    while (search_in_progress) {
        // Запрашиваем участок для исследования
        Message request_msg;
        memset(&request_msg, 0, sizeof(request_msg));
        request_msg.action = ACTION_REQUEST;
        request_msg.swarm_id = swarm_id;
        
        if (mq_send(dispatch_queue, (char*)&request_msg, sizeof(request_msg), 0) == -1) {
            perror("mq_send");
            break;
        }
        
        std::cout << "Стая #" << swarm_id << ": запрашивает участок для исследования" << std::endl;
        
        // Ожидаем ответ от диспетчера
        Message response_msg;
        bool got_response = false;
        
        // Ждем ответ, предназначенный именно для нашей стаи
        while (!got_response && search_in_progress) {
            ssize_t received = mq_receive(dispatch_queue, (char*)&response_msg, sizeof(Message), nullptr);
            
            if (received <= 0) {
                perror("mq_receive");
                break;
            }
            
            std::cout << "Стая #" << swarm_id << ": получила сообщение с action=" << response_msg.action 
                      << ", swarm_id=" << response_msg.swarm_id << std::endl;
                      
            // Проверяем, что сообщение адресовано нам или это общее сообщение о завершении
            if (response_msg.swarm_id == swarm_id) {
                got_response = true;
            }
            else if (response_msg.action == ACTION_FINISH && response_msg.swarm_id == 0) {
                std::cout << "Стая #" << swarm_id << ": получила общее сообщение о завершении поиска" << std::endl;
                search_in_progress = false;
                break;
            }
            else {
                // Если сообщение не для нас, возвращаем его обратно в очередь
                if (mq_send(dispatch_queue, (char*)&response_msg, sizeof(response_msg), 0) == -1) {
                    perror("mq_send (returning message)");
                }
                // Небольшая пауза перед следующей попыткой
                usleep(100000); // 100ms
            }
        }
        
        if (!got_response || !search_in_progress) {
            continue; // Если не получили свой ответ или поиск завершен, пробуем снова
        }
        
        // Теперь у нас есть ответ именно для нашей стаи
        if (response_msg.action == ACTION_FINISH) { // Нет больше участков или поиск завершен
            std::cout << "Стая #" << swarm_id << ": получила информацию о завершении поиска" << std::endl;
            break;
        }
        else if (response_msg.action == ACTION_ASSIGN) {
            // Исследуем полученный участок
            int area_to_search = response_msg.area;
            int winnie_area = response_msg.winnie_area;
            
            std::cout << "Стая #" << swarm_id << ": исследует участок " << area_to_search << std::endl;
            
            // Имитация поиска
            int search_time = search_time_dist(gen);
            sleep(search_time);
            
            // Подготавливаем сообщение о результате
            Message result_msg;
            memset(&result_msg, 0, sizeof(result_msg));
            result_msg.swarm_id = swarm_id;
            result_msg.area = area_to_search;
            
            // Проверяем, находится ли Винни-Пух на этом участке
            if (area_to_search == winnie_area) {
                std::cout << "Стая #" << swarm_id << ": НАШЛА Винни-Пуха на участке " << area_to_search << "!" << std::endl;
                std::cout << "Винни-Пух получает наказание от стаи #" << swarm_id << std::endl;
                
                result_msg.action = ACTION_FOUND;
                if (mq_send(dispatch_queue, (char*)&result_msg, sizeof(result_msg), 0) == -1) {
                    perror("mq_send");
                }
                break;
            } else {
                std::cout << "Стая #" << swarm_id << ": НЕ обнаружила Винни-Пуха на участке " << area_to_search << std::endl;
                
                result_msg.action = ACTION_EXPLORED;
                if (mq_send(dispatch_queue, (char*)&result_msg, sizeof(result_msg), 0) == -1) {
                    perror("mq_send");
                }
            }
            
            // Задержка перед возвращением в улей
            sleep(1);
            std::cout << "Стая #" << swarm_id << ": возвращается в улей" << std::endl;
            
            // Небольшая пауза в улье перед новым вылетом
            sleep(1);
        }
        else {
            std::cout << "Стая #" << swarm_id << ": получила неизвестный тип сообщения (" << response_msg.action 
                      << "). Игнорирую." << std::endl;
        }
    }
    
    std::cout << "Стая #" << swarm_id << ": завершила поиски и вернулась в улей" << std::endl;
    cleanup();
    
    return 0;
}
