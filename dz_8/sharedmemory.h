#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <semaphore.h>
#include <fcntl.h>           // Для констант O_*
#include <sys/stat.h>        // Для режима доступа

const char *SHM_NAME = "/my_shared_memory";
const char *SEM_WRITER_NAME = "/writer_semaphore";
const char *SEM_READER_NAME = "/reader_semaphore";
const size_t BUFFER_SIZE = 100;       // Размер кольцевого буфера
const size_t MAX_READERS = 2;        // Максимальное число читателей

struct SharedMemoryBuffer {
    int buffer[BUFFER_SIZE];    // Кольцевой буфер
    size_t write_index;         // Индекс записи
    size_t read_index;          // Индекс чтения
    size_t reader_count;        // Количество текущих читателей
    bool data_available;        // Флаг наличия данных
    bool writer_active;         // Флаг активности писателя
};

#endif // SHARED_MEMORY_H

