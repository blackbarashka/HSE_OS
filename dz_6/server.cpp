#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>

#define SHM_KEY 0x1234
#define SEM_KEY 0x5678

struct SharedData {
    int number;
    int exit_flag;
};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int shm_id;
int sem_id;
SharedData *shm_ptr;

void cleanup(int sig) {
    std::cout << "\nCleaning up resources..." << std::endl;
    shmdt(shm_ptr);
    shmctl(shm_id, IPC_RMID, nullptr);
    semctl(sem_id, 0, IPC_RMID);
    exit(EXIT_SUCCESS);
}

void init_semaphores(int sem_id) {
    union semun arg;
    unsigned short values[2] = {1, 0};
    arg.array = values;
    if (semctl(sem_id, 0, SETALL, arg) == -1) {
        perror("semctl SETALL");
        exit(EXIT_FAILURE);
    }
}

int main() {
    shm_id = shmget(SHM_KEY, sizeof(SharedData), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        return EXIT_FAILURE;
    }

    shm_ptr = (SharedData*)shmat(shm_id, nullptr, 0);
    if (shm_ptr == (void*)-1) {
        perror("shmat");
        shmctl(shm_id, IPC_RMID, nullptr);
        return EXIT_FAILURE;
    }

    shm_ptr->exit_flag = 0;

    sem_id = semget(SEM_KEY, 2, IPC_CREAT | 0666);
    if (sem_id == -1) {
        perror("semget");
        shmdt(shm_ptr);
        shmctl(shm_id, IPC_RMID, nullptr);
        return EXIT_FAILURE;
    }

    init_semaphores(sem_id);
    signal(SIGINT, cleanup);

    std::cout << "Server started. Waiting for client..." << std::endl;

    while (true) {
        struct sembuf sops = {1, -1, 0};
        if (semop(sem_id, &sops, 1) == -1) {
            perror("semop server wait");
            break;
        }

        if (shm_ptr->exit_flag) {
            std::cout << "Client requested exit. Exiting..." << std::endl;
            break;
        }

        std::cout << "Received number: " << shm_ptr->number << std::endl;

        sops = {0, 1, 0};
        if (semop(sem_id, &sops, 1) == -1) {
            perror("semop client post");
            break;
        }
    }

    cleanup(0);
    return EXIT_SUCCESS;
}
