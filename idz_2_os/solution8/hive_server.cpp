#include <iostream>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <cstdlib>
#include <ctime>
#include <string>

#define SHM_KEY 0x1234
#define SEM_KEY 0x5678
#define MAX_AREAS 100

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

// Функция для очистки всех ресурсов IPC
void cleanup() {
    // Удаляем семафоры
    if (sem_id != -1) {
        semctl(sem_id, 0, IPC_RMID);
    }
    
    // Отключаем и удаляем разделяемую память
    if (shared_data && shm_id != -1) {
        shmdt(shared_data);
        shmctl(shm_id, IPC_RMID, nullptr);
    }
    
    std::cout << "Ресурсы очищены" << std::endl;
}

// Обработчик сигналов для корректного завершения
void signal_handler(int sig) {
    std::cout << "\nПолучен сигнал завершения. Останавливаем поиски..." << std::endl;
    if (shared_data) {
        shared_data->searching = false;
    }
    sleep(1); // Даем время другим процессам увидеть изменение
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
    if (num_areas > MAX_AREAS) num_areas = MAX_AREAS;
    
    // Создаем и получаем идентификатор разделяемой памяти
    shm_id = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        exit(1);
    }
    
    // Подключаемся к разделяемой памяти
    shared_data = (SharedData*)shmat(shm_id, nullptr, 0);
    if (shared_data == (void*)-1) {
        perror("shmat");
        shmctl(shm_id, IPC_RMID, nullptr);
        exit(1);
    }
    
    // Инициализируем данные
    shared_data->total_areas = num_areas;
    shared_data->areas_explored = 0;
    shared_data->next_area = 0;
    shared_data->winnie_found = false;
    shared_data->searching = true;
    
    // Случайно выбираем участок, где спрятался Винни-Пух
    shared_data->winnie_area = rand() % num_areas;
    
    // Создаем набор семафоров
    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("semget");
        shmdt(shared_data);
        shmctl(shm_id, IPC_RMID, nullptr);
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
    
    std::cout << "========== ПОИСК ВИННИ-ПУХА ==========" << std::endl;
    std::cout << "Сервер улья запущен!" << std::endl;
    std::cout << "Всего участков леса: " << num_areas << std::endl;
    std::cout << "Винни-Пух спрятался на участке " << shared_data->winnie_area << std::endl;
    std::cout << "=======================================" << std::endl;
    
    // Главный цикл сервера улья
    while (shared_data->searching) {
        // Проверяем статус поисков
        sem_operation(sem_id, SEM_MUTEX, -1); // Захватываем семафор
        
        bool all_areas_explored = (shared_data->areas_explored >= shared_data->total_areas);
        bool winnie_found = shared_data->winnie_found;
        
        sem_operation(sem_id, SEM_MUTEX, 1); // Освобождаем семафор
        
        // Если все участки исследованы или Винни-Пух найден, завершаем поиски
        if (all_areas_explored || winnie_found) {
            shared_data->searching = false;
            break;
        }
        
        // Пауза для снижения нагрузки на процессор
        sleep(1);
    }
    
    // Ждем некоторое время, чтобы все стаи пчел успели завершить работу
    sleep(3);
    
    // Выводим итоговую информацию
    std::cout << "\n========== РЕЗУЛЬТАТЫ ПОИСКА ==========" << std::endl;
    if (shared_data->winnie_found) {
        std::cout << "Поиски завершены! Винни-Пух найден на участке " << shared_data->winnie_area << " и наказан!" << std::endl;
    } else {
        std::cout << "Поиски завершены! Все " << shared_data->areas_explored << " участков исследованы." << std::endl;
        std::cout << "Винни-Пуха не нашли, хотя он был на участке " << shared_data->winnie_area << "." << std::endl;
    }
    std::cout << "=======================================" << std::endl;
    
    // Очищаем ресурсы
    cleanup();
    
    return 0;
}
