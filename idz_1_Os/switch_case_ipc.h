// switch_case_ipc.h
#ifndef SWITCH_CASE_IPC_H
#define SWITCH_CASE_IPC_H

#include <string>

void sender(const std::string &input_file, int msgid);
void processor(int msgid, int msgid_out);
void receiver(const std::string &output_file, int msgid);

#endif // SWITCH_CASE_IPC_H
