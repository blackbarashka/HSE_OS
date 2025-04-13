#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <ctime>

#define SHM_KEY 0x1234
#define SEM_KEY 0x5678

struct SharedData {
    int number;
    int exit_flag;
    pid_t server_pid; // Добавлено поле для PID сервера
};

int shm_id;
int sem_id;
SharedData *shm_ptr;

void cleanup(int sig) {
    std::cout << "\nExiting client..." << std::endl;
    shmdt(shm_ptr);
    exit(EXIT_SUCCESS);
}

int main() {
    shm_id = shmget(SHM_KEY, sizeof(SharedData), 0666);
    if (shm_id == -1) {
        perror("shmget");
        return EXIT_FAILURE;
    }

    shm_ptr = (SharedData*)shmat(shm_id, nullptr, 0);
    if (shm_ptr == (void*)-1) {
        perror("shmat");
        return EXIT_FAILURE;
    }

    sem_id = semget(SEM_KEY, 2, 0666);
    if (sem_id == -1) {
        perror("semget");
        shmdt(shm_ptr);
        return EXIT_FAILURE;
    }

    signal(SIGINT, [](int sig) {
        pid_t server_pid = shm_ptr->server_pid;
        std::cout << "\nSending SIGUSR1 to server (PID: " << server_pid << ")..." << std::endl;
        kill(server_pid, SIGUSR1); // Отправляем SIGUSR1 серверу
        cleanup(sig);
    });

    srand(time(nullptr));

    while (true) {
        int num = rand() % 100;

        struct sembuf sops = {0, -1, 0};
        if (semop(sem_id, &sops, 1) == -1) {
            perror("semop client wait");
            break;
        }

        shm_ptr->number = num;

        sops = {1, 1, 0};
        if (semop(sem_id, &sops, 1) == -1) {
            perror("semop server post");
            break;
        }

        sleep(1);
    }

    shmdt(shm_ptr);
    return EXIT_SUCCESS;
}
