// switch_case_ipc.cpp
#include "switch_case_ipc.h"
#include <iostream>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " input.txt output.txt\n";
        return 1;
    }

    key_t key1 = ftok("sender", 65);
    key_t key2 = ftok("processor", 66);
    int msgid1 = msgget(key1, 0666 | IPC_CREAT);
    int msgid2 = msgget(key2, 0666 | IPC_CREAT);

    if (fork() == 0) {
        sender(argv[1], msgid1);
        exit(0);
    }
    if (fork() == 0) {
        processor(msgid1, msgid2);
        exit(0);
    }
    receiver(argv[2], msgid2);

    msgctl(msgid1, IPC_RMID, nullptr);
    msgctl(msgid2, IPC_RMID, nullptr);
    return 0;
}
