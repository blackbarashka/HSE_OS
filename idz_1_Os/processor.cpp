#include "switch_case_ipc.h"
#include <iostream>
#include <cctype>
#include <sys/msg.h>

struct message {
    long msg_type;
    char text[128];
};

void switch_case(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (islower(str[i]))
            str[i] = toupper(str[i]);
        else if (isupper(str[i]))
            str[i] = tolower(str[i]);
    }
}

void processor(int msgid, int msgid_out) {
    message msg;
    while (true) {
        msgrcv(msgid, &msg, sizeof(msg.text), 1, 0);
        if (msg.text[0] == '\0') break;
        switch_case(msg.text);
        msgsnd(msgid_out, &msg, sizeof(msg.text), 0);
    }
    msgsnd(msgid_out, &msg, 1, 0);
}
