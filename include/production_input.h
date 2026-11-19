#ifndef PRODUCTION_INPUT_H
#define PRODUCTION_INPUT_H

#include "project_contract.h"

void reception_process(int write_fd);
void cleaning_process(int read_fd, int write_fd);
void washing_process(int read_fd, int write_fd);

#endif
