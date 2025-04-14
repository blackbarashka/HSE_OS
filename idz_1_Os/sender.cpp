#include "switch_case_ipc.h"
#include <iostream>
#include <fstream>
#include <sys/msg.h>

struct message {
    long msg_type;
    char text[128];
};

void sender(const std::string &input_file, int msgid) {
    std::ifstream file(input_file);
    if (!file) {
        std::cerr << "Ошибка открытия файла: " << input_file << "\n";
        exit(1);
    }
    message msg;
    msg.msg_type = 1;
    while (file.read(msg.text, sizeof(msg.text) - 1) || file.gcount() > 0) {
        msg.text[file.gcount()] = '\0';
        msgsnd(msgid, &msg, file.gcount() + 1, 0);
    }
    msg.text[0] = '\0';
    msgsnd(msgid, &msg, 1, 0);
}
