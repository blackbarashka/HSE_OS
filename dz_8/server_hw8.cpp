#include <iostream>
#include <unistd.h>
#include <sys/mman.h>
#include "sharedmemory.h"

int main() {
    // Открываем разделяемую память
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Ошибка при открытии разделяемой памяти");
        return 1;
    }

    // Отображаем разделяемую память в адресное пространство процесса
    SharedMemoryBuffer *shared_buffer = (SharedMemoryBuffer *)mmap(nullptr, sizeof(SharedMemoryBuffer),
                                                                   PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_buffer == MAP_FAILED) {
        perror("Ошибка при отображении разделяемой памяти");
        return 1;
    }

    // Открываем семафоры
    sem_t *writer_sem = sem_open(SEM_WRITER_NAME, 0);
    if (writer_sem == SEM_FAILED) {
        perror("Ошибка при открытии семафора писателя");
        return 1;
    }

    sem_t *reader_sem = sem_open(SEM_READER_NAME, 0);
    if (reader_sem == SEM_FAILED) {
        perror("Ошибка при открытии семафора читателя");
        return 1;
    }

    // Регистрация нового читателя
    sem_wait(reader_sem);
    if (shared_buffer->reader_count >= MAX_READERS) {
        std::cerr << "Превышено максимальное число читателей" << std::endl;
        sem_post(reader_sem);
        return 1;
    }
    ++shared_buffer->reader_count;
    sem_post(reader_sem);

    // Чтение данных из буфера
    while (true) {
        // Проверяем, остались ли активные писатели
        if (!shared_buffer->writer_active && !shared_buffer->data_available) {
            break;
        }

        // Захватываем семафор читателя
        sem_wait(reader_sem);

        if (shared_buffer->data_available) {
            // Чтение из буфера
            int num = shared_buffer->buffer[shared_buffer->read_index];
            shared_buffer->read_index = (shared_buffer->read_index + 1) % BUFFER_SIZE;

            // Проверка, есть ли еще данные
            if (shared_buffer->read_index == shared_buffer->write_index) {
                shared_buffer->data_available = false;
            }

            std::cout << "Читатель (" << getpid() << ") прочитал: " << num << std::endl;
        }

        // Освобождаем семафор читателя
        sem_post(reader_sem);
    }

    // Отписываемся от чтения
    sem_wait(reader_sem);
    --shared_buffer->reader_count;
    sem_post(reader_sem);

    // Если это был последний читатель, удаляем разделяемую память и семафоры
    if (shared_buffer->reader_count == 0 && !shared_buffer->writer_active) {
        munmap(shared_buffer, sizeof(SharedMemoryBuffer));
        shm_unlink(SHM_NAME);
        sem_close(writer_sem);
        sem_close(reader_sem);
        sem_unlink(SEM_WRITER_NAME);
        sem_unlink(SEM_READER_NAME);
        std::cout << "Сервер завершил работу и освободил ресурсы" << std::endl;
    } else {
        // Закрываем семафоры и отменяем отображение разделяемой памяти
        sem_close(writer_sem);
        sem_close(reader_sem);
        munmap(shared_buffer, sizeof(SharedMemoryBuffer));
    }

    return 0;
}

