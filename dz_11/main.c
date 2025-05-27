#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_PATH_LEN 1024
#define MAX_ATTEMPTS 100

int main() {
    char dirname[] = "symlink_test_dir";
    char base_filename[] = "a";
    char base_path[MAX_PATH_LEN];
    char link_path[MAX_PATH_LEN];
    char next_link_path[MAX_PATH_LEN];
    int depth = 0;
    int fd;

    // Удаляем предыдущую директорию, если существует
    char rm_command[MAX_PATH_LEN];
    snprintf(rm_command, MAX_PATH_LEN, "rm -rf %s", dirname);
    system(rm_command);

    // Создаем директорию для тестов
    if (mkdir(dirname, 0755) == -1) {
        perror("Ошибка создания директории");
        return 1;
    }

    // Формируем полный путь к базовому файлу
    snprintf(base_path, MAX_PATH_LEN, "%s/%s", dirname, base_filename);
    
    // Создаем базовый файл
    fd = open(base_path, O_CREAT | O_WRONLY, 0644);
    if (fd == -1) {
        perror("Ошибка создания файла");
        return 1;
    }
    
    // Записываем в файл некоторое содержимое
    if (write(fd, "test content", 12) == -1) {
        perror("Ошибка записи в файл");
        close(fd);
        return 1;
    }
    
    close(fd);
    printf("Базовый файл создан: %s\n", base_path);
    
    // Имя первой символической ссылки
    snprintf(link_path, MAX_PATH_LEN, "%s/link_0", dirname);
    
    // Создаем первую символическую ссылку на базовый файл
    if (symlink(base_filename, link_path) == -1) {
        perror("Ошибка создания первой символической ссылки");
        return 1;
    }
    
    // Проверяем, можем ли открыть первую ссылку
    fd = open(link_path, O_RDONLY);
    if (fd == -1) {
        printf("Не удалось открыть первую ссылку: %s\n", strerror(errno));
        return 1;
    }
    close(fd);
    depth = 1;
    
    // Начинаем создавать цепочку символических ссылок
    for (int i = 1; i < MAX_ATTEMPTS; i++) {
        // Формируем имя новой символической ссылки
        snprintf(next_link_path, MAX_PATH_LEN, "%s/link_%d", dirname, i);
        
        // Имя предыдущей ссылки (относительное)
        char prev_link_name[32];
        snprintf(prev_link_name, 32, "link_%d", i - 1);
        
        // Создаем символическую ссылку на предыдущую ссылку
        if (symlink(prev_link_name, next_link_path) == -1) {
            printf("Не удалось создать ссылку глубиной %d: %s\n", i + 1, strerror(errno));
            break;
        }
        
        // Пытаемся открыть файл через новую символическую ссылку
        fd = open(next_link_path, O_RDONLY);
        if (fd == -1) {
            printf("Не удалось открыть ссылку глубиной %d: %s\n", i + 1, strerror(errno));
            break;
        }
        
        close(fd);
        depth++;
        
        strcpy(link_path, next_link_path);
    }
    
    printf("Максимальная глубина рекурсии символических ссылок: %d\n", depth);
    printf("Примечание: Все тестовые файлы созданы в директории %s\n", dirname);
    printf("Для удаления директории выполните команду: rm -rf %s\n", dirname);
    
    return 0;
}
