#ifndef PRODUCTION_OUTPUT_H
#define PRODUCTION_OUTPUT_H

#include "project_contract.h"

void crushing_process(int read_fd, int write_fd);
void malaxation_process(int read_fd, int write_fd);
void separation_process(int read_fd);
#endif
