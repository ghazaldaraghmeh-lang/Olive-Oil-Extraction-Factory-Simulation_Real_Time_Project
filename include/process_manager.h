#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <sys/types.h>
#include "project_contract.h"
#include "config.h"

/* Create all factory processes */
int create_factory_processes(
    ProcessInfo processes[],
    int pipes[][2],
    int qc_pipe[2],
    const FactoryConfig *config
);

/* Close pipe file descriptors */
void close_all_pipes(
    int pipes[][2],
    int qc_pipe[2]
);

/* Wait for all child processes */
void wait_for_processes(
    ProcessInfo processes[]
);

#endif
