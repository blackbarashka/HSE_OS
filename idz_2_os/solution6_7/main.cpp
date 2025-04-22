#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <ctime>
#include <cstring>
#include <random>
#include <string>

#define SHM_NAME "/winnie_search_shm_unnamed"
#define MAX_AREAS 100

// Структура для хранения общих данных, включая неименованный семафор
struct SharedData {
    int total_areas;          // Общее количество участков в лесу
    int areas_explored;       // Количество уже исследованных участков
    int next_area;            // Следующий участок для исследования
    bool winnie_found;        // Флаг нахождения Винни-Пуха
    int winnie_area;          // Участок, где находится Винни-Пух
    bool searching;           // Флаг продолжения поиска
    sem_t mutex;              // Неименованный семафор для взаимного исключения
};

// Глобальные переменные для очистки ресурсов
int shm_fd = -1;
SharedData* shared_data = nullptr;

// Функция для очистки всех ресурсов
void cleanup() {
    // Уничтожаем семафор
    if (shared_data) {
        sem_destroy(&shared_data->mutex);
        munmap(shared_data, sizeof(SharedData));
    }

    // Удаляем разделяемую память
    if (shm_fd != -1) {
        close(shm_fd);
        shm_unlink(SHM_NAME);
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

// Функция для стаи пчел
void bee_swarm(int swarm_id) {
    std::random_device rd;
    std::mt19937 gen(rd() + swarm_id); // Используем id роя для уникального сида
    std::uniform_int_distribution<> search_time_dist(1, 5);
    std::cout << "Стая пчел #" << swarm_id << " вылетела из улья." << std::endl;

    while (shared_data->searching) {
        // Получаем участок для исследования
        sem_wait(&shared_data->mutex);
        if (shared_data->areas_explored >= shared_data->total_areas || shared_data->winnie_found || !shared_data->searching) {
            // Все участки исследованы или Винни-Пух уже найден
            sem_post(&shared_data->mutex);
            break;
        }

        int area_to_search = shared_data->next_area++;
        shared_data->areas_explored++;
        sem_post(&shared_data->mutex);

        // Исследуем участок
        std::cout << "Стая пчел #" << swarm_id << " исследует участок " << area_to_search << std::endl;

        // Время поиска
        int search_time = search_time_dist(gen);
        sleep(search_time);

        // Проверяем, не нашел ли кто-то Винни-Пуха, пока мы искали
        sem_wait(&shared_data->mutex);
        if (shared_data->winnie_found) {
            sem_post(&shared_data->mutex);
            std::cout << "Стая пчел #" << swarm_id << " узнала, что Винни-Пух уже найден!" << std::endl;
            break;
        }

        // Проверяем, находится ли Винни-Пух на этом участке
        if (area_to_search == shared_data->winnie_area) {
            shared_data->winnie_found = true;
            std::cout << "Стая пчел #" << swarm_id << " НАШЛА Винни-Пуха на участке " << area_to_search << "!" << std::endl;
            std::cout << "Винни-Пух получает наказание от стаи #" << swarm_id << std::endl;
            sem_post(&shared_data->mutex);
            break;
        }

        std::cout << "Стая пчел #" << swarm_id << " не обнаружила Винни-Пуха на участке " << area_to_search << std::endl;
        sem_post(&shared_data->mutex);

        // Задержка перед возвращением в улей
        sleep(1);
        std::cout << "Стая пчел #" << swarm_id << " возвращается в улей." << std::endl;

        // Короткая пауза в улье
        sleep(1);
    }

    std::cout << "Стая пчел #" << swarm_id << " завершила поиски и вернулась в улей." << std::endl;
    exit(0);
}

int main(int argc, char* argv[]) {
    srand(time(nullptr));

    // Устанавливаем обработчик сигналов
    signal(SIGINT, signal_handler);
    
    // Определяем параметры поиска
    int num_swarms = 5; // По умолчанию 5 стай пчел
    int num_areas = 20; // По умолчанию 20 участков леса
    
    // Проверяем наличие аргументов командной строки
    if (argc > 1) {
        num_swarms = std::stoi(argv[1]);
    }
    if (argc > 2) {
        num_areas = std::stoi(argv[2]);
    }

    // Проверяем корректность параметров
    if (num_swarms <= 0) num_swarms = 5;
    if (num_areas <= 0) num_areas = 20;
    if (num_areas > MAX_AREAS) num_areas = MAX_AREAS;

    // Создаем и инициализируем разделяемую память
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shm_fd, sizeof(SharedData)) == -1) {
        perror("ftruncate");
        close(shm_fd);
        shm_unlink(SHM_NAME);
        exit(1);
    }

    shared_data = static_cast<SharedData*>(mmap(nullptr, sizeof(SharedData),
                                              PROT_READ | PROT_WRITE, MAP_SHARED,
                                              shm_fd, 0));

    if (shared_data == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        shm_unlink(SHM_NAME);
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

    // Инициализируем неименованный семафор в разделяемой памяти
    if (sem_init(&shared_data->mutex, 1, 1) == -1) {
        perror("sem_init");
        munmap(shared_data, sizeof(SharedData));
        close(shm_fd);
        shm_unlink(SHM_NAME);
        exit(1);
    }

    std::cout << "========== ПОИСК ВИННИ-ПУХА ==========" << std::endl;
    std::cout << "Всего стай пчел: " << num_swarms << std::endl;
    std::cout << "Всего участков леса: " << num_areas << std::endl;
    std::cout << "Винни-Пух спрятался на участке " << shared_data->winnie_area << std::endl;
    std::cout << "=======================================" << std::endl;
    // Создаем процессы для стай пчел
    std::vector<pid_t> swarm_pids;
    for (int i = 0; i < num_swarms; ++i) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            cleanup();
            exit(1);
        }
        else if (pid == 0) {
            // Дочерний процесс - стая пчел
            bee_swarm(i);
            // Не дойдем сюда, так как bee_swarm завершается через exit()
        }
        else {
            // Родительский процесс
            swarm_pids.push_back(pid);
        }
    }
    // Ожидаем завершения всех стай пчел
    for (pid_t pid : swarm_pids) {
        waitpid(pid, nullptr, 0);
    }
    // Проверяем результат поисков
    if (shared_data->winnie_found) {
        std::cout << "Поиски завершены! Винни-Пух найден на участке " << shared_data->winnie_area << " и наказан!" << std::endl;
    } else {
        std::cout << "Поиски завершены! Винни-Пуха не нашли, хотя он был на участке " << shared_data->winnie_area << "." << std::endl;
    }
    // Очищаем ресурсы
    cleanup();

    return 0;
}
