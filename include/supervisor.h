#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include "project_contract.h"

int supervisor_init(void);

void supervisor_run(ProcessInfo processes[]);

void supervisor_shutdown(ProcessInfo processes[]);

#endif
