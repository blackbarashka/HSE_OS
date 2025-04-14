// receiver.cpp
#include "switch_case_ipc.h"
#include <iostream>
#include <fstream>
#include <sys/msg.h>

struct message {
    long msg_type;
    char text[128];
};

void receiver(const std::string &output_file, int msgid) {
    std::ofstream file(output_file);
    if (!file) {
        std::cerr << "Ошибка открытия файла: " << output_file << "\n";
        exit(1);
    }
    message msg;
    while (true) {
        msgrcv(msgid, &msg, sizeof(msg.text), 1, 0);
        if (msg.text[0] == '\0') break;
        file << msg.text;
    }
}
