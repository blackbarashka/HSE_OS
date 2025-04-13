
#include <iostream>
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>

volatile sig_atomic_t transmission_ended = 0;
volatile sig_atomic_t bit_received = 0;
char received_bit = 0;

pid_t transmitter_pid;

void bit0_handler(int signum) {
    received_bit = '0';
    bit_received = 1;
}

void bit1_handler(int signum) {
    received_bit = '1';
    bit_received = 1;
}

void transmission_end_handler(int signum) {
    transmission_ended = 1;
}

int main() {
    // Выдаем PID.
    pid_t own_pid = getpid();
    std::cout << "Receiver PID: " << own_pid << std::endl;

    // Получаем PID отправителя.
    std::cout << "Enter Transmitter PID: ";
    std::cin >> transmitter_pid;

    struct sigaction sa_bit0, sa_bit1, sa_end;
    memset(&sa_bit0, 0, sizeof(sa_bit0));
    sa_bit0.sa_handler = bit0_handler;
    sigaction(SIGUSR1, &sa_bit0, NULL);

    memset(&sa_bit1, 0, sizeof(sa_bit1));
    sa_bit1.sa_handler = bit1_handler;
    sigaction(SIGUSR2, &sa_bit1, NULL);

    memset(&sa_end, 0, sizeof(sa_end));
    sa_end.sa_handler = transmission_end_handler;
    sigaction(SIGINT, &sa_end, NULL);

    sigset_t mask_bit, oldmask;
    sigemptyset(&mask_bit);
    sigaddset(&mask_bit, SIGUSR1);
    sigaddset(&mask_bit, SIGUSR2);
    sigaddset(&mask_bit, SIGINT);

    sigprocmask(SIG_BLOCK, &mask_bit, &oldmask);

    //Получаем биты.
    std::vector<char> received_bits;
    while (!transmission_ended) {
        bit_received = 0;

        while (!bit_received && !transmission_ended) {
            sigsuspend(&oldmask);
        }

        if (transmission_ended) {
            break;
        }


        received_bits.push_back(received_bit);

        if (kill(transmitter_pid, SIGUSR1) == -1) {
            perror("Error sending acknowledgment to transmitter");
            exit(EXIT_FAILURE);
        }
    }

    // Конвертируем биты в число.
    int32_t received_number = 0;
    int total_bits = received_bits.size();

    for (size_t i = 0; i < received_bits.size(); ++i) {
        received_number <<= 1;
        if (received_bits[i] == '1') {
            received_number |= 1;
        }
    }
    if (total_bits == 32 && (received_number & 0x80000000)) {
    }

    std::cout << "Received integer: " << received_number << std::endl;
    return 0;
}
