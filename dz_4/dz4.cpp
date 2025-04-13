#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cerrno>
#include <cstring>

int main(int argc, char *argv[]) {
    // Проверяем правильность количества аргументов командной строки
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <файл_для_чтения> <файл_для_записи>" << std::endl;
        return EXIT_FAILURE;
    }
    
    const char *input_file = argv[1];
    const char *output_file = argv[2];

    // Открываем входной файл только для чтения
    int fd_in = open(input_file, O_RDONLY);
    if (fd_in < 0) {
        std::cerr << "Ошибка при открытии входного файла '" << input_file << "': " << strerror(errno) << std::endl;
        return EXIT_FAILURE;
    }

    // Получаем информацию о входном файле
    struct stat st;
    if (fstat(fd_in, &st) < 0) {
        std::cerr << "Ошибка при получении информации о файле '" << input_file << "': " << strerror(errno) << std::endl;
        close(fd_in);
        return EXIT_FAILURE;
    }

    // Определяем права доступа для выходного файла
    mode_t output_mode = st.st_mode & 0777; // Извлекаем биты разрешений

    // Проверяем, является ли входной файл исполняемым для любого из пользователей
    if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
        // Исполняемый файл: оставляем биты исполнений
    } else {
        // Неисполнимый файл: убираем биты исполнений
        output_mode &= ~(S_IXUSR | S_IXGRP | S_IXOTH);
    }

    // Открываем (или создаем) выходной файл с определенными правами доступа
    int fd_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, output_mode);
    if (fd_out < 0) {
        std::cerr << "Ошибка при открытии/создании выходного файла '" << output_file << "': " << strerror(errno) << std::endl;
        close(fd_in);
        return EXIT_FAILURE;
    }

    // Используем буфер ограниченного размера для циклического чтения и записи
    const size_t buffer_size = 1024; // Буфер размером 1 КБ
    char buffer[buffer_size];
    ssize_t bytes_read;

    // Читаем из входного файла и записываем в выходной файл в цикле
    while ((bytes_read = read(fd_in, buffer, buffer_size)) > 0) {
        ssize_t bytes_written = 0;
        const char *write_ptr = buffer;

        // Гарантируем, что все прочитанные байты будут записаны
        while (bytes_written < bytes_read) {
            ssize_t result = write(fd_out, write_ptr + bytes_written, bytes_read - bytes_written);
            if (result < 0) {
                std::cerr << "Ошибка при записи в выходной файл '" << output_file << "': " << strerror(errno) << std::endl;
                close(fd_in);
                close(fd_out);
                return EXIT_FAILURE;
            }
            bytes_written += result;
        }
    }

    if (bytes_read < 0) {
        std::cerr << "Ошибка при чтении из входного файла '" << input_file << "': " << strerror(errno) << std::endl;
        close(fd_in);
        close(fd_out);
        return EXIT_FAILURE;
    }

    // Закрываем файловые дескрипторы
    if (close(fd_in) < 0) {
        std::cerr << "Ошибка при закрытии входного файла '" << input_file << "': " << strerror(errno) << std::endl;
        close(fd_out);
        return EXIT_FAILURE;
    }

    if (close(fd_out) < 0) {
        std::cerr << "Ошибка при закрытии выходного файла '" << output_file << "': " << strerror(errno) << std::endl;
        return EXIT_FAILURE;
    }

    // Устанавливаем права доступа на выходной файл явно (на случай, если umask их изменил)
    if (chmod(output_file, output_mode) < 0) {
        std::cerr << "Ошибка при установке прав доступа на выходной файл '" << output_file << "': " << strerror(errno) << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
