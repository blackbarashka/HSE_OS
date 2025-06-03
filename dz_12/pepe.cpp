#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <csignal>
#include <cstring>
#include <string>

// Идентификаторы семафоров в наборе
#define PARENT_SEM 0
#define CHILD_SEM 1

// Флаг для завершения работы по сигналу
volatile sig_atomic_t shouldTerminate = 0;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void signalHandler(int signo) {
    shouldTerminate = 1;
}

void semSignal(int semid, int semNum) {
    struct sembuf op;
    op.sem_num = semNum;
    op.sem_op = 1;  // Увеличить счетчик на 1
    op.sem_flg = 0;
    
    if (semop(semid, &op, 1) == -1) {
        perror("semop: signal error");
        exit(1);
    }
}

void semWait(int semid, int semNum) {
    struct sembuf op;
    op.sem_num = semNum;
    op.sem_op = -1;  // Уменьшить счетчик на 1
    op.sem_flg = 0;
    
    if (semop(semid, &op, 1) == -1) {
        if (errno == EINTR && shouldTerminate) {
            return;
        }
        perror("semop: wait error");
        exit(1);
    }
}

int main() {
    // Установка обработчика сигнала
    struct sigaction sa;
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    
    int pipefd[2];  // Дескрипторы канала
    pid_t childpid; // PID дочернего процесса
    int semid;      // ID набора семафоров
    
    // Создание канала
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    
    // Создание набора семафоров (2 семафора)
    semid = semget(IPC_PRIVATE, 2, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }
    
    // Инициализация семафоров
    union semun arg;
    
    // Родительскому процессу разрешено начать (установлен в 1)
    arg.val = 1;
    if (semctl(semid, PARENT_SEM, SETVAL, arg) == -1) {
        perror("semctl: SETVAL PARENT_SEM");
        exit(EXIT_FAILURE);
    }
    
    // Дочерний процесс ждет (установлен в 0)
    arg.val = 0;
    if (semctl(semid, CHILD_SEM, SETVAL, arg) == -1) {
        perror("semctl: SETVAL CHILD_SEM");
        exit(EXIT_FAILURE);
    }
    
    // Создание дочернего процесса
    childpid = fork();
    if (childpid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    // Дочерний процесс
    if (childpid == 0) {
        std::cout << "[Child] Process started with PID: " << getpid() << std::endl;
        
        char buffer[100];
        int count = 0;
        
        while (!shouldTerminate) {
            // Ожидаем, когда родительский процесс разрешит дочернему читать
            semWait(semid, CHILD_SEM);
            if (shouldTerminate) break;
            
            // Чтение сообщения от родителя
            memset(buffer, 0, sizeof(buffer));
            ssize_t bytesRead = read(pipefd[0], buffer, sizeof(buffer));
            if (bytesRead <= 0) {
                if (bytesRead == 0) {
                    std::cout << "[Child] Pipe closed by parent" << std::endl;
                } else {
                    perror("[Child] read error");
                }
                break;
            }
            
            std::cout << "[Child] Received: " << buffer << std::endl;
            count++;
            
            // Подготовка к записи
            std::string response = "Message from child #" + std::to_string(count);
            
            // Запись ответа
            if (write(pipefd[1], response.c_str(), response.length() + 1) == -1) {
                perror("[Child] write error");
                break;
            }
            
            std::cout << "[Child] Sent: " << response << std::endl;
            
            // Пауза в 1 секунду
            sleep(1);
            
            // Разрешаем родительскому процессу читать
            semSignal(semid, PARENT_SEM);
            
            // Если получен сигнал прерывания, завершаем работу
            if (shouldTerminate) break;
        }
        
        std::cout << "[Child] Process is terminating..." << std::endl;
        close(pipefd[0]);
        close(pipefd[1]);
        
        exit(EXIT_SUCCESS);
    }
    
    // Родительский процесс
    else {
        std::cout << "[Parent] Process started with PID: " << getpid() << std::endl;
        std::cout << "[Parent] Created child with PID: " << childpid << std::endl;
        std::cout << "[Parent] Press Ctrl+C to terminate the communication" << std::endl;
        
        char buffer[100];
        int count = 0;
        
        while (!shouldTerminate) {
            // Ожидаем, когда можно писать (начальное значение семафора = 1)
            semWait(semid, PARENT_SEM);
            if (shouldTerminate) break;
            
            // Подготовка сообщения
            std::string message = "Message from parent #" + std::to_string(++count);
            
            // Отправка сообщения
            if (write(pipefd[1], message.c_str(), message.length() + 1) == -1) {
                perror("[Parent] write error");
                break;
            }
            
            std::cout << "[Parent] Sent: " << message << std::endl;
            
            // Разрешаем дочернему процессу читать
            semSignal(semid, CHILD_SEM);
            
            // Ожидаем, когда дочерний процесс отправит ответ
            semWait(semid, PARENT_SEM);
            if (shouldTerminate) break;
            
            // Чтение ответа от дочернего процесса
            memset(buffer, 0, sizeof(buffer));
            ssize_t bytesRead = read(pipefd[0], buffer, sizeof(buffer));
            if (bytesRead <= 0) {
                if (bytesRead == 0) {
                    std::cout << "[Parent] Pipe closed by child" << std::endl;
                } else {
                    perror("[Parent] read error");
                }
                break;
            }
            
            std::cout << "[Parent] Received: " << buffer << std::endl;
            
            // Пауза в 1 секунду
            sleep(1);
            
            // Если получен сигнал прерывания, завершаем работу
            if (shouldTerminate) break;
            
            // Родитель снова готов к отправке сообщения
            semSignal(semid, PARENT_SEM);
        }
        
        std::cout << "[Parent] Process is terminating..." << std::endl;
        
        // Закрываем канал
        close(pipefd[0]);
        close(pipefd[1]);
        
        // Ожидаем завершения дочернего процесса
        waitpid(childpid, NULL, 0);
        
        // Удаляем набор семафоров
        if (semctl(semid, 0, IPC_RMID) == -1) {
            perror("[Parent] semctl: IPC_RMID");
        } else {
            std::cout << "[Parent] Semaphore set removed successfully" << std::endl;
        }
        
        std::cout << "[Parent] Exit" << std::endl;
        exit(EXIT_SUCCESS);
    }
    
    return 0;
}
