#define _POSIX_C_SOURCE 200809L

#include "../include/production_output.h"
#include "../include/config.h"
#include "../include/messages.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_batch(int fd, Batch *batch)
{
    unsigned char *bytes = (unsigned char *)batch;
    size_t received = 0;
    while (received < sizeof(*batch)) {
        ssize_t n = read(fd, bytes + received, sizeof(*batch) - received);
        if (n > 0) {
            received += (size_t)n;
        } else if (n == 0) {
            if (received == 0) return 0;
            fprintf(stderr, "[Malaxation] Incomplete Batch received.\n");
            return -1;
        } else if (errno != EINTR) {
            perror("[Malaxation] read");
            return -1;
        }
    }
    return 1;
}

static int write_batch(int fd, const Batch *batch)
{
    const unsigned char *bytes = (const unsigned char *)batch;
    size_t sent = 0;
    while (sent < sizeof(*batch)) {
        ssize_t n = write(fd, bytes + sent, sizeof(*batch) - sent);
        if (n > 0) {
            sent += (size_t)n;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            if (n < 0) perror("[Malaxation] write");
            else fprintf(stderr, "[Malaxation] Could not write Batch.\n");
            return -1;
        }
    }
    return 0;
}

static void report_to_supervisor(int type, const Batch *batch, const char *detail)
{
    ProcessMessage msg = {0};
    msg.process_id = getpid();
    msg.stage_id = STAGE_MALAXATION;
    msg.message_type = type;
    msg.batch_id = batch ? batch->batch_id : -1;
    msg.temperature = batch ? batch->temperature : 0.0f;
    msg.pressure = batch ? batch->pressure : 0.0f;
    msg.status = batch ? batch->status : BATCH_ERROR;
    snprintf(msg.message, sizeof(msg.message), "%s", detail);

    int fd = open(STATUS_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {

        if (errno != ENOENT && errno != ENXIO)
            perror("[Malaxation] open Supervisor FIFO");
        return;
    }
    ssize_t n;
    do {
        n = write(fd, &msg, sizeof(msg));
    } while (n < 0 && errno == EINTR);
    if (n != (ssize_t)sizeof(msg)) {
        if (n < 0) perror("[Malaxation] write Supervisor FIFO");
        else fprintf(stderr, "[Malaxation] Incomplete Supervisor report.\n");
    }
    close(fd);
}

static int get_malaxation_config(FactoryConfig *config)
{
    set_default_config(config);
    const char *filename = getenv("OLIVE_CONFIG_FILE");
    if (!filename || !filename[0]) filename = DEFAULT_CONFIG_FILE;
    if (load_config(filename, config) != 0) {
        fprintf(stderr, "[Malaxation] Cannot load '%s'; using defaults.\n", filename);
    }
    if (config->malaxation_time < 0 ||
        !isfinite(config->max_temperature) || config->max_temperature <= 0.0f ||
        !isfinite(config->max_pressure) || config->max_pressure <= 0.0f) {
        fprintf(stderr, "[Malaxation] Invalid factory configuration.\n");
        return -1;
    }
    return 0;
}

void malaxation_process(int read_fd, int write_fd)
{
    Batch batch;
    FactoryConfig config;
    int result = 0;

    struct sigaction ignore_sigpipe = {0};
    struct sigaction old_sigpipe;
    int have_old_sigpipe = 0;
    ignore_sigpipe.sa_handler = SIG_IGN;
    sigemptyset(&ignore_sigpipe.sa_mask);
    if (sigaction(SIGPIPE, &ignore_sigpipe, &old_sigpipe) == 0)
        have_old_sigpipe = 1;

    if (get_malaxation_config(&config) != 0) {
        report_to_supervisor(MSG_ERROR, NULL, "Malaxation configuration invalid");
        goto cleanup;
    }

    while ((result = read_batch(read_fd, &batch)) == 1) {
        if (batch.status == BATCH_REJECTED || batch.status == BATCH_ERROR) {

            report_to_supervisor(MSG_WARNING, &batch,
                                 "Batch failed earlier; Malaxation skipped");
        } else {
            batch.current_stage = STAGE_MALAXATION;
            batch.status = BATCH_IN_PROGRESS;

            if (!isfinite(batch.olive_weight) || batch.olive_weight <= 0.0f) {
                batch.status = BATCH_ERROR;
                report_to_supervisor(MSG_ERROR, &batch, "Invalid olive weight");
            } else if (!isfinite(batch.temperature) ||
                       !isfinite(batch.pressure) ||
                       batch.temperature > config.max_temperature ||
                       batch.pressure > config.max_pressure) {
                batch.status = BATCH_ERROR;
                report_to_supervisor(MSG_ERROR, &batch,
                                     "Mixing temperature or pressure unsafe");
            } else if (batch.processing_time < 0 ||
                       batch.processing_time > INT_MAX - config.malaxation_time) {
                batch.status = BATCH_ERROR;
                report_to_supervisor(MSG_ERROR, &batch,
                                     "Invalid cumulative processing time");
            } else {
                report_to_supervisor(MSG_STAGE_STARTED, &batch, "Malaxation started");

                unsigned int remaining = (unsigned int)config.malaxation_time;
                while (remaining > 0) remaining = sleep(remaining);

                batch.processing_time += config.malaxation_time;
                report_to_supervisor(MSG_STAGE_COMPLETED, &batch,
                                     "Malaxation completed; paste ready for Separation");
                printf("[Malaxation] Batch %d: %.2f kg mixed in %d s.\n",
                       batch.batch_id, batch.olive_weight, config.malaxation_time);
                fflush(stdout);
            }
        }

        if (write_batch(write_fd, &batch) != 0) {
            report_to_supervisor(MSG_ERROR, &batch, "Could not forward Batch");
            break;
        }
    }
    if (result < 0)
        report_to_supervisor(MSG_ERROR, NULL, "Malaxation input pipe read failed");
cleanup:
    close(read_fd);
    close(write_fd);
    if (have_old_sigpipe) sigaction(SIGPIPE, &old_sigpipe, NULL);
}
