#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <sys/wait.h>
#include <cstdlib>
#include <dirent.h>
#include <cstdint>

using namespace std;
//Фибонначи.
uint64_t fibonacci(uint64_t n) {
    uint64_t a = 0, b = 1, c;
    if (n == 0) return a;
    for (uint64_t i = 2; i <= n; i++) {
        c = a + b;
        if (c < b) { // Переполнение.
            cerr << "Обнаружено переполнение при вычислении числа Фибоначчи." << endl;
            exit(EXIT_FAILURE);
        }
        a = b;
        b = c;
    }
    return b;
}

//Факториал.
uint64_t factorial(uint64_t n) {
    uint64_t result = 1;
    for (uint64_t i = 1; i <= n; i++) {
        uint64_t temp = result * i;
        if (temp / i != result) { // Переполнение.
            cerr << "Обнаружено переполнение при вычислении факториала." << endl;
            exit(EXIT_FAILURE);
        }
        result = temp;
    }
    return result;
}

//Вывод каталога.
void list_directory() {
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(".")) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            cout << ent->d_name << '\n';
        }
        closedir(dir);
    } else {
        perror("Ошибка при открытии каталога");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cerr << "Использование: " << argv[0] << " <число>" << endl;
        return EXIT_FAILURE;
    }

    uint64_t n = strtoull(argv[1], nullptr, 10);

    pid_t pid = fork();
    if (pid < 0) {
        perror("Ошибка при вызове fork");
        return EXIT_FAILURE;
    }

    // Дочерний процесс.
    if (pid == 0) {
        cout << "Дочерний процесс (PID: " << getpid() << ", Родительский PID: " << getppid() << ")" << endl;
        uint64_t fact = factorial(n);
        cout << "Факториал числа " << n << " равен " << fact << endl;
        exit(EXIT_SUCCESS);
    }

    // Родительский процесс.
    cout << "Родительский процесс (PID: " << getpid() << ")" << endl;
    uint64_t fib = fibonacci(n);
    cout << "Число Фибоначчи для " << n << " равно " << fib << endl;

    // Ожидание завершения дочернего процесса.
    wait(nullptr);

    // Процесс для вывода содержимого текущего каталога.
    if (fork() == 0) {
        cout << "Процесс для вывода содержимого каталога (PID: " << getpid() << ", Родительский PID: " << getppid() << ")" << endl;
        list_directory();
        exit(EXIT_SUCCESS);
    }

    // Ожидание завершения процесса вывода каталога.
    wait(nullptr);

    return EXIT_SUCCESS;
}
