#include <iostream>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/mman.h>
#include "sharedmemory.h"

int main() {
    // Открываем разделяемую память
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Ошибка при создании разделяемой памяти");
        return 1;
    }

    // Устанавливаем размер разделяемой памяти
    ftruncate(shm_fd, sizeof(SharedMemoryBuffer));

    // Отображаем разделяемую память в адресное пространство процесса
    SharedMemoryBuffer *shared_buffer = (SharedMemoryBuffer *)mmap(nullptr, sizeof(SharedMemoryBuffer),
                                                                   PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_buffer == MAP_FAILED) {
        perror("Ошибка при отображении разделяемой памяти");
        return 1;
    }

    // Инициализация разделяемой памяти при первом запуске
    shared_buffer->write_index = 0;
    shared_buffer->read_index = 0;
    shared_buffer->reader_count = 0;
    shared_buffer->data_available = false;
    shared_buffer->writer_active = true;

    // Открываем семафоры
    sem_t *writer_sem = sem_open(SEM_WRITER_NAME, O_CREAT, 0666, 1);
    if (writer_sem == SEM_FAILED) {
        perror("Ошибка при создании семафора писателя");
        return 1;
    }

    sem_t *reader_sem = sem_open(SEM_READER_NAME, O_CREAT, 0666, 1);
    if (reader_sem == SEM_FAILED) {
        perror("Ошибка при создании семафора читателя");
        return 1;
    }

    // Генерация случайных чисел и запись в буфер
    std::srand(std::time(nullptr));
    for (int i = 0; i < 50; ++i) {
        int num = std::rand() % 1000;  // Генерируем случайное число

        // Захватываем семафор писателя
        sem_wait(writer_sem);

        // Запись в буфер
        shared_buffer->buffer[shared_buffer->write_index] = num;
        shared_buffer->write_index = (shared_buffer->write_index + 1) % BUFFER_SIZE;
        shared_buffer->data_available = true;

        std::cout << "Писатель записал: " << num << std::endl;

        // Освобождаем семафор писателя
        sem_post(writer_sem);

      }

    // Завершаем работу писателя
    shared_buffer->writer_active = false;

    // Закрываем семафоры
    sem_close(writer_sem);
    sem_close(reader_sem);

    // Отменяем отображение разделяемой памяти
    munmap(shared_buffer, sizeof(SharedMemoryBuffer));
    close(shm_fd);

    return 0;
}

