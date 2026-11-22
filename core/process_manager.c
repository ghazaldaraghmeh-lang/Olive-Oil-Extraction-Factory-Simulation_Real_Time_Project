#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#include "process_manager.h"
#include "production_input.h"
#include "production_output.h"
#include "quality_control.h"
#include "messages.h"

#define TIMING_FILE "/tmp/olive_factory_timing"

static double get_elapsed_time(struct timespec start,
                               struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec)
         + (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

/*
 * Store the execution time of one stage.
 */
static void record_stage_timing(const char *stage_name,
                                struct timespec start,
                                struct timespec end)
{
    if (!stage_name)
        return;

    double elapsed = get_elapsed_time(start, end);

    int fd = open(TIMING_FILE,
                  O_WRONLY | O_CREAT | O_APPEND,
                  0644);

    if (fd < 0)
        return;

    char buffer[256];

    int len = snprintf(buffer,
                       sizeof(buffer),
                       "%s|%.6f\n",
                       stage_name,
                       elapsed);

    if (len > 0) {
        ssize_t written;

        do {
            written = write(fd, buffer, (size_t)len);
        } while (written < 0 && errno == EINTR);
    }

    close(fd);
}

static void reset_child_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void close_unused_pipes(int pipes[][2], int qc_pipe[2],
                               int keep_read, int keep_write)
{
    for (int i = 0; i < NUM_PIPES; i++) {
        if (i != keep_read && i != keep_write) {
            if (pipes[i][0] >= 0) close(pipes[i][0]);
            if (pipes[i][1] >= 0) close(pipes[i][1]);
        }
    }

    if (qc_pipe[0] >= 0) close(qc_pipe[0]);
    if (qc_pipe[1] >= 0) close(qc_pipe[1]);
}

static void cleanup_created_processes(ProcessInfo processes[])
{
    for (int i = 0; i < STAGE_SUPERVISOR; i++) {
        if (processes[i].pid > 0)
            kill(processes[i].pid, SIGTERM);
    }

    for (int i = 0; i < STAGE_SUPERVISOR; i++) {
        if (processes[i].pid > 0) {
            while (waitpid(processes[i].pid, NULL, 0) == -1) {
                if (errno == EINTR)
                    continue;

                if (errno == ECHILD)
                    break;

                break;
            }

            processes[i].pid = -1;
        }
    }
}

static void send_qc_result(const Batch *batch,
                           int message_type,
                           const char *text)
{
    if (!batch)
        return;

    ProcessMessage msg = {0};

    msg.process_id = getpid();
    msg.stage_id = STAGE_QUALITY_CONTROL;
    msg.message_type = message_type;
    msg.batch_id = batch->batch_id;
    msg.temperature = batch->temperature;
    msg.pressure = batch->pressure;
    msg.oil_quantity = batch->oil_quantity;
    msg.water_quantity = batch->water_quantity;
    msg.pomace_quantity = batch->pomace_quantity;
    msg.status = batch->status;

    snprintf(msg.message,
             sizeof(msg.message),
             "%s",
             text);

    int fd = open(STATUS_FIFO,
                  O_WRONLY | O_NONBLOCK);

    if (fd < 0)
        return;

    ssize_t n;

    do {
        n = write(fd, &msg, sizeof(msg));
    } while (n < 0 && errno == EINTR);

    (void)n;

    close(fd);
}

int create_factory_processes(ProcessInfo processes[],
                             int pipes[][2],
                             int qc_pipe[2],
                             const FactoryConfig *config)
{
    if (!processes || !pipes || !qc_pipe || !config)
        return -1;

    for (int i = 0; i < NUM_PIPES; i++) {
        pipes[i][0] = -1;
        pipes[i][1] = -1;
    }

    qc_pipe[0] = qc_pipe[1] = -1;

    for (int i = 0; i < NUM_STAGES; i++) {
        processes[i].pid = -1;
        processes[i].stage = i;
    }

    for (int i = 0; i < NUM_PIPES; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("[Process Manager] pipe");
            close_all_pipes(pipes, qc_pipe);
            return -1;
        }
    }

    if (pipe(qc_pipe) == -1) {
        perror("[Process Manager] QC pipe");
        close_all_pipes(pipes, qc_pipe);
        return -1;
    }

    pid_t pid;

    /*
     * --------------------------------------------------------
     * RECEPTION
     * --------------------------------------------------------
     */
    pid = fork();

    if (pid == -1)
        goto fork_error;

    if (pid == 0) {
        struct timespec start;
        struct timespec end;

        reset_child_signals();

        close_unused_pipes(pipes, qc_pipe, -1, 0);

        close(pipes[0][0]);

        clock_gettime(CLOCK_MONOTONIC, &start);

        reception_process(pipes[0][1]);

        clock_gettime(CLOCK_MONOTONIC, &end);

        record_stage_timing("RECEPTION", start, end);

        _exit(EXIT_SUCCESS);
    }

    processes[STAGE_RECEPTION].pid = pid;

    /*
     * --------------------------------------------------------
     * CLEANING
     * --------------------------------------------------------
     */
    pid = fork();

    if (pid == -1)
        goto fork_error;

    if (pid == 0) {
        struct timespec start;
        struct timespec end;

        reset_child_signals();

        close_unused_pipes(pipes, qc_pipe, 0, 1);

        close(pipes[0][1]);
        close(pipes[1][0]);

        clock_gettime(CLOCK_MONOTONIC, &start);

        cleaning_process(pipes[0][0],
                         pipes[1][1]);

        clock_gettime(CLOCK_MONOTONIC, &end);

        record_stage_timing("CLEANING", start, end);

        _exit(EXIT_SUCCESS);
    }

    processes[STAGE_CLEANING].pid = pid;

    /*
     * --------------------------------------------------------
     * WASHING
     * --------------------------------------------------------
     */
    pid = fork();

    if (pid == -1)
        goto fork_error;

    if (pid == 0) {
        struct timespec start;
        struct timespec end;

        reset_child_signals();

        close_unused_pipes(pipes, qc_pipe, 1, 2);

        close(pipes[1][1]);
        close(pipes[2][0]);

        clock_gettime(CLOCK_MONOTONIC, &start);

        washing_process(pipes[1][0],
                        pipes[2][1]);

        clock_gettime(CLOCK_MONOTONIC, &end);

        record_stage_timing("WASHING", start, end);

        _exit(EXIT_SUCCESS);
    }

    processes[STAGE_WASHING].pid = pid;

    /*
     * --------------------------------------------------------
     * CRUSHING
     * --------------------------------------------------------
     */
    pid = fork();

    if (pid == -1)
        goto fork_error;

    if (pid == 0) {
        struct timespec start;
        struct timespec end;

        reset_child_signals();

        close_unused_pipes(pipes, qc_pipe, 2, 3);

        close(pipes[2][1]);
        close(pipes[3][0]);

        clock_gettime(CLOCK_MONOTONIC, &start);

        crushing_process(pipes[2][0],
                         pipes[3][1]);

        clock_gettime(CLOCK_MONOTONIC, &end);

        record_stage_timing("CRUSHING", start, end);

        _exit(EXIT_SUCCESS);
    }

    processes[STAGE_CRUSHING].pid = pid;

    /*
     * --------------------------------------------------------
     * MALAXATION
     * --------------------------------------------------------
     */
    pid = fork();

    if (pid == -1)
        goto fork_error;

    if (pid == 0) {
        struct timespec start;
        struct timespec end;

        reset_child_signals();

        close_unused_pipes(pipes, qc_pipe, 3, 4);

        close(pipes[3][1]);
        close(pipes[4][0]);

        clock_gettime(CLOCK_MONOTONIC, &start);

        malaxation_process(pipes[3][0],
                           pipes[4][1]);

        clock_gettime(CLOCK_MONOTONIC, &end);

        record_stage_timing("MALAXATION", start, end);

        _exit(EXIT_SUCCESS);
    }

    processes[STAGE_MALAXATION].pid = pid;

    /*
     * --------------------------------------------------------
     * SEPARATION
     * --------------------------------------------------------
     */
    pid = fork();

    if (pid == -1)
        goto fork_error;

    if (pid == 0) {
        struct timespec start;
        struct timespec end;

        reset_child_signals();

        for (int i = 0; i < NUM_PIPES; i++) {
            if (i != 4) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }
        }

        close(pipes[4][1]);
        close(qc_pipe[0]);

        clock_gettime(CLOCK_MONOTONIC, &start);

        separation_process(pipes[4][0],
                           qc_pipe[1]);

        clock_gettime(CLOCK_MONOTONIC, &end);

        record_stage_timing("SEPARATION", start, end);

        _exit(EXIT_SUCCESS);
    }

    processes[STAGE_SEPARATION].pid = pid;

    /*
     * --------------------------------------------------------
     * QUALITY CONTROL
     * --------------------------------------------------------
     */
    pid = fork();

    if (pid == -1)
        goto fork_error;

    if (pid == 0) {
        struct timespec start;
        struct timespec end;

        reset_child_signals();

        Batch batch;

        close(qc_pipe[1]);

        for (int i = 0; i < NUM_PIPES; i++) {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }

        clock_gettime(CLOCK_MONOTONIC, &start);

        while (read(qc_pipe[0],
                    &batch,
                    sizeof(Batch)) == sizeof(Batch)) {

            int score = calculate_quality_score(&batch);

            print_quality_report(&batch, score);

            QualityResult result = check_batch_quality(
                &batch,
                config->min_quality_score,
                config->max_temperature,
                config->max_pressure
            );

            if (result == QUALITY_ACCEPTED) {

                batch.status = BATCH_COMPLETED;

                printf("[Quality Control] Batch %d ACCEPTED\n",
                       batch.batch_id);

                send_qc_result(
                    &batch,
                    MSG_BATCH_ACCEPTED,
                    "Quality Control: batch ACCEPTED"
                );

            } else {

                batch.status = BATCH_REJECTED;

                printf("[Quality Control] Batch %d REJECTED\n",
                       batch.batch_id);

                send_qc_result(
                    &batch,
                    MSG_BATCH_REJECTED,
                    "Quality Control: batch REJECTED"
                );
            }

            fflush(stdout);
        }

        close(qc_pipe[0]);

        clock_gettime(CLOCK_MONOTONIC, &end);

        record_stage_timing("QUALITY CONTROL",
                            start,
                            end);

        _exit(EXIT_SUCCESS);
    }

    processes[STAGE_QUALITY_CONTROL].pid = pid;

    close_all_pipes(pipes, qc_pipe);

    return 0;

fork_error:

    perror("[Process Manager] fork");

    cleanup_created_processes(processes);

    close_all_pipes(pipes, qc_pipe);

    return -1;
}

void close_all_pipes(int pipes[][2],
                     int qc_pipe[2])
{
    for (int i = 0; i < NUM_PIPES; i++) {

        if (pipes[i][0] >= 0) {
            close(pipes[i][0]);
            pipes[i][0] = -1;
        }

        if (pipes[i][1] >= 0) {
            close(pipes[i][1]);
            pipes[i][1] = -1;
        }
    }

    if (qc_pipe[0] >= 0) {
        close(qc_pipe[0]);
        qc_pipe[0] = -1;
    }

    if (qc_pipe[1] >= 0) {
        close(qc_pipe[1]);
        qc_pipe[1] = -1;
    }
}

void wait_for_processes(ProcessInfo processes[])
{
    if (!processes)
        return;

    for (int i = 0; i < STAGE_SUPERVISOR; i++) {

        if (processes[i].pid > 0) {

            while (waitpid(processes[i].pid,
                           NULL,
                           0) == -1) {

                if (errno == EINTR)
                    continue;

                if (errno == ECHILD)
                    break;

                perror("[Process Manager] waitpid");
                break;
            }

            processes[i].pid = -1;
        }
    }
}