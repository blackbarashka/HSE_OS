#include <iostream>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <cstdlib>
#include <ctime>
#include <random>

#define SHM_KEY 0x1234
#define SEM_KEY 0x5678

// Определение семафоров
#define SEM_MUTEX 0  // Для взаимного исключения

// Определение структуры для разделяемой памяти
struct SharedData {
    int total_areas;        // Общее количество участков
    int areas_explored;     // Исследованные участки
    int next_area;          // Следующий участок для поиска
    bool winnie_found;      // Флаг нахождения Винни-Пуха
    int winnie_area;        // Участок, на котором находится Винни-Пух
    bool searching;         // Флаг продолжения поиска
};

// Глобальные переменные для очистки ресурсов
int shm_id = -1;
int sem_id = -1;
SharedData* shared_data = nullptr;

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

// Функция для освобождения ресурсов
void cleanup() {
    if (shared_data) {
        shmdt(shared_data);
    }
}

// Обработчик сигналов
void signal_handler(int sig) {
    std::cout << "\nПолучен сигнал завершения. Стая возвращается в улей..." << std::endl;
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
    
    // Получаем доступ к разделяемой памяти
    shm_id = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (shm_id == -1) {
        perror("shmget");
        std::cerr << "Не удается подключиться к улью. Возможно, сервер не запущен." << std::endl;
        exit(1);
    }
    
    // Подключаемся к разделяемой памяти
    shared_data = (SharedData*)shmat(shm_id, nullptr, 0);
    if (shared_data == (void*)-1) {
        perror("shmat");
        exit(1);
    }
    
    // Получаем доступ к набору семафоров
    sem_id = semget(SEM_KEY, 1, 0666);
    if (sem_id == -1) {
        perror("semget");
        cleanup();
        exit(1);
    }
    
    std::cout << "Стая пчел #" << swarm_id << " вылетела из улья." << std::endl;
    
    // Цикл поиска Винни-Пуха
    while (shared_data->searching) {
        // Получаем участок для исследования
        sem_operation(sem_id, SEM_MUTEX, -1); // Захватываем семафор
        
        if (shared_data->areas_explored >= shared_data->total_areas || 
            shared_data->winnie_found || 
            !shared_data->searching) {
            // Все участки исследованы или Винни-Пух уже найден
            sem_operation(sem_id, SEM_MUTEX, 1); // Освобождаем семафор
            break;
        }
        
        int area_to_search = shared_data->next_area++;
        shared_data->areas_explored++;
        
        sem_operation(sem_id, SEM_MUTEX, 1); // Освобождаем семафор
        
        // Исследуем участок
        std::cout << "Стая пчел #" << swarm_id << " исследует участок " << area_to_search << std::endl;
        
        // Время поиска
        int search_time = search_time_dist(gen);
        sleep(search_time);
        
        // Проверяем, не нашел ли кто-то Винни-Пуха, пока мы искали
        sem_operation(sem_id, SEM_MUTEX, -1);
        
        if (shared_data->winnie_found) {
            sem_operation(sem_id, SEM_MUTEX, 1);
            std::cout << "Стая пчел #" << swarm_id << " узнала, что Винни-Пух уже найден!" << std::endl;
            break;
        }
        
        // Проверяем, находится ли Винни-Пух на этом участке
        if (area_to_search == shared_data->winnie_area) {
            shared_data->winnie_found = true;
            std::cout << "Стая пчел #" << swarm_id << " НАШЛА Винни-Пуха на участке " << area_to_search << "!" << std::endl;
            std::cout << "Винни-Пух получает наказание от стаи #" << swarm_id << std::endl;
            sem_operation(sem_id, SEM_MUTEX, 1);
            break;
        }
        
        std::cout << "Стая пчел #" << swarm_id << " не обнаружила Винни-Пуха на участке " << area_to_search << std::endl;
        sem_operation(sem_id, SEM_MUTEX, 1);
        
        // Задержка перед возвращением в улей
        sleep(1);
        std::cout << "Стая пчел #" << swarm_id << " возвращается в улей." << std::endl;
        
        // Короткая пауза в улье
        sleep(1);
    }
    
    std::cout << "Стая пчел #" << swarm_id << " завершила поиски и вернулась в улей." << std::endl;
    
    // Освобождаем ресурсы
    cleanup();
    
    return 0;
}
