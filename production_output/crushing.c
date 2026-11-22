#define _POSIX_C_SOURCE 200809L

#include "../include/production_output.h"
#include "../include/config.h"
#include "../include/messages.h"
#include "../include/sensors.h"

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
    unsigned char *data = (unsigned char *)batch;
    size_t received = 0;

    while (received < sizeof(*batch)) {
        ssize_t n = read(fd, data + received, sizeof(*batch) - received);
        if (n > 0) {
            received += (size_t)n;
        } else if (n == 0) {
            if (received == 0) return 0;
            fprintf(stderr, "[Crushing] Incomplete Batch received.\n");
            return -1;
        } else if (errno != EINTR) {
            perror("[Crushing] read");
            return -1;
        }
    }
    return 1;
}

static int write_batch(int fd, const Batch *batch)
{
    const unsigned char *data = (const unsigned char *)batch;
    size_t sent = 0;

    while (sent < sizeof(*batch)) {
        ssize_t n = write(fd, data + sent, sizeof(*batch) - sent);
        if (n > 0) {
            sent += (size_t)n;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            if (n < 0) perror("[Crushing] write");
            else fprintf(stderr, "[Crushing] Could not write Batch.\n");
            return -1;
        }
    }
    return 0;
}

static void report_to_supervisor(int type, const Batch *batch, const char *detail)
{
    ProcessMessage msg = {0};
    msg.process_id = getpid();
    msg.stage_id = STAGE_CRUSHING;
    msg.message_type = type;
    msg.batch_id = batch ? batch->batch_id : -1;
    msg.temperature = batch ? batch->temperature : 0.0f;
    msg.pressure = batch ? batch->pressure : 0.0f;
    msg.status = batch ? batch->status : BATCH_ERROR;
    snprintf(msg.message, sizeof(msg.message), "%s", detail);

    int fd = open(STATUS_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {

        if (errno != ENOENT && errno != ENXIO)
            perror("[Crushing] opening Supervisor FIFO");
        return;
    }

    ssize_t n;
    do {
        n = write(fd, &msg, sizeof(msg));
    } while (n < 0 && errno == EINTR);

    if (n != (ssize_t)sizeof(msg)) {
        if (n < 0) perror("[Crushing] Supervisor FIFO write");
        else fprintf(stderr, "[Crushing] Incomplete Supervisor message.\n");
    }
    close(fd);
}

static int get_crushing_config(FactoryConfig *config)
{
    set_default_config(config);

    const char *filename = getenv("OLIVE_CONFIG_FILE");
    if (!filename || !filename[0]) filename = DEFAULT_CONFIG_FILE;

    if (load_config(filename, config) != 0) {
        fprintf(stderr,
                "[Crushing] Cannot load '%s'; using default configuration.\n",
                filename);
    }

    if (config->crushing_time < 0 || config->max_temperature <= 0.0f ||
        config->max_pressure <= 0.0f ||
        !isfinite(config->max_temperature) ||
        !isfinite(config->max_pressure)) {
        fprintf(stderr, "[Crushing] Invalid factory configuration.\n");
        return -1;
    }
    return 0;
}

void crushing_process(int read_fd, int write_fd)
{
    FactoryConfig config;
    Batch batch;
    int result = 0;

    struct sigaction ignore_sigpipe = {0};
    struct sigaction old_sigpipe;
    int have_old_sigpipe = 0;
    ignore_sigpipe.sa_handler = SIG_IGN;
    sigemptyset(&ignore_sigpipe.sa_mask);
    if (sigaction(SIGPIPE, &ignore_sigpipe, &old_sigpipe) == 0)
        have_old_sigpipe = 1;

    if (get_crushing_config(&config) != 0) {
        report_to_supervisor(MSG_ERROR, NULL, "Crushing configuration invalid");
        goto cleanup;
    }

    while ((result = read_batch(read_fd, &batch)) == 1) {
        if (batch.status == BATCH_REJECTED || batch.status == BATCH_ERROR) {

            report_to_supervisor(MSG_WARNING, &batch,
                                 "Batch already failed before Crushing; skipped");
        } else {
            batch.current_stage = STAGE_CRUSHING;
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
                                     "Temperature or pressure exceeds permitted range");
            } else if (batch.processing_time < 0 ||
                       batch.processing_time > INT_MAX - config.crushing_time) {
                batch.status = BATCH_ERROR;
                report_to_supervisor(MSG_ERROR, &batch, "Invalid total processing time");
            } else {
                report_to_supervisor(MSG_STAGE_STARTED, &batch, "Crushing started");

                unsigned int remaining = (unsigned int)config.crushing_time;
                while (remaining > 0) remaining = sleep(remaining);

                batch.processing_time += config.crushing_time;
                update_stage_sensors(&batch, STAGE_CRUSHING, &config);
                report_to_supervisor(MSG_STAGE_COMPLETED, &batch,
                                     "Crushing completed; olive paste ready");
                printf("[Crushing] Batch %d: %.2f kg crushed in %d s.\n",
                       batch.batch_id, batch.olive_weight, config.crushing_time);
                fflush(stdout);
            }
        }

        if (write_batch(write_fd, &batch) != 0) {
            report_to_supervisor(MSG_ERROR, &batch, "Could not forward Batch");
            break;
        }
    }

    if (result < 0)
        report_to_supervisor(MSG_ERROR, NULL, "Crushing input pipe read failed");

cleanup:
    close(read_fd);
    close(write_fd);
    if (have_old_sigpipe) sigaction(SIGPIPE, &old_sigpipe, NULL);
}
