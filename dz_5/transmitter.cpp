
#include <iostream>
#include <unistd.h> // Для getpid().
#include <csignal>
#include <cstdlib>
#include <cstring> // Для memset().
#include <sys/types.h>
#include <sys/wait.h>
#include <cerrno> // Для errno.

volatile sig_atomic_t integer = 0;
void acknowledgment_handler(int signum) {
    integer = 1;
}

int main() {
    // Выдаем PID.
    pid_t own_pid = getpid();
    std::cout << "Transmitter PID: " << own_pid << std::endl;
    // Получаем PID приемника.
    pid_t receiver_pid;
    std::cout << "ВВедите Receiver PID: ";
    std::cin >> receiver_pid;

    // Запращиваем число.
    int32_t number_to_send;
    std::cout << "Enter an integer to send: ";
    std::cin >> number_to_send;

    // Обработчик сигнала.
    struct sigaction sa_ack;
    memset(&sa_ack, 0, sizeof(sa_ack));
    sa_ack.sa_handler = acknowledgment_handler;
    sigaction(SIGUSR1, &sa_ack, NULL);
    sigset_t mask_ack, oldmask;
    sigemptyset(&mask_ack);
    sigaddset(&mask_ack, SIGUSR1);

    // блокируем SIGUSR1
    sigprocmask(SIG_BLOCK, &mask_ack, &oldmask);

    // Отправка.
    int total_bits = sizeof(number_to_send) * 8;
    for (int i = total_bits - 1; i >= 0; --i) {
        int bit = (number_to_send >> i) & 1;
        int sig_to_send = (bit == 0) ? SIGUSR1 : SIGUSR2;
        if (kill(receiver_pid, sig_to_send) == -1) {
            perror("Error sending signal to receiver");
            exit(EXIT_FAILURE);
        }
        integer = 0;
        while (!integer) {
            sigsuspend(&oldmask);
        }
    }

    if (kill(receiver_pid, SIGINT) == -1) {
        perror("Error sending SIGINT to receiver");
        exit(EXIT_FAILURE);
    }

    std::cout << "Transmission complete." << std::endl;
    return 0;
}
