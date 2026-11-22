#define _POSIX_C_SOURCE 200809L

#include "production_input.h"
#include "messages.h"
#include "config.h"
#include "sensors.h"

#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

// Reception creates batches from the olive container and sends them to Cleaning.

#define DEFAULT_CONTAINER_BATCHES 3

static void report_event(int type, int batch_id, int status, const char *text)
{
    ProcessMessage message = {0};
    message.process_id = getpid();
    message.stage_id = STAGE_RECEPTION;
    message.message_type = type;
    message.batch_id = batch_id;
    message.status = status;
    snprintf(message.message, sizeof(message.message), "%s", text);

    static int warned = 0;
    int fd = open(STATUS_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        if (!warned) {
            fprintf(stderr, "[Reception] Supervisor FIFO unavailable; reports may be missed.\n");
            warned = 1;
        }
        return;
    }
    warned = 0;
    if (write(fd, &message, sizeof(message)) != (ssize_t)sizeof(message)) {
        fprintf(stderr, "[Reception] Failed to send status report.\n");
    }
    close(fd);
}

static void report_sensor(const Batch *batch)
{
    if (batch == NULL) return;
    ProcessMessage message = {0};
    message.process_id = getpid();
    message.stage_id = STAGE_RECEPTION;
    message.message_type = MSG_SENSOR_UPDATE;
    message.batch_id = batch->batch_id;
    message.temperature = batch->temperature;
    message.pressure = batch->pressure;
    message.status = batch->status;
    snprintf(message.message, sizeof(message.message),
             "Sensor update: temperature %.1f C, pressure %.1f Bar",
             batch->temperature, batch->pressure);
    int fd = open(STATUS_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd < 0) return;
    ssize_t n;
    do { n = write(fd, &message, sizeof(message)); } while (n < 0 && errno == EINTR);
    (void)n;
    close(fd);
}

static int write_batch(int fd, const Batch *batch)
{
    const char *data = (const char *)batch;
    size_t written = 0;

    while (written < sizeof(*batch)) {
        ssize_t n = write(fd, data + written, sizeof(*batch) - written);
        if (n > 0) {
            written += (size_t)n;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            if (n == 0) errno = EIO;
            return -1;
        }
    }
    return 0;
}

static int get_total_olives(double batch_size, double *total)
{
    const char *text = getenv("OLIVE_TOTAL_KG");
    if (text == NULL || *text == '\0') {
        *total = batch_size * DEFAULT_CONTAINER_BATCHES;
        return isfinite(*total) && *total <= FLT_MAX ? 0 : -1;
    }

    char *end;
    errno = 0;
    double value = strtod(text, &end);
    if (text == end || errno == ERANGE || *end != '\0' ||
        !isfinite(value) || value < 0.0 || value > FLT_MAX) {
        return -1;
    }
    *total = value;
    return 0;
}

void reception_process(int write_fd)
{
    struct sigaction ignore = {0}, previous;
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    int changed = (sigaction(SIGPIPE, &ignore, &previous) == 0);

    if (write_fd < 0) {
        report_event(MSG_ERROR, 0, BATCH_ERROR, "Invalid Reception output Pipe");
        goto done;
    }

    const char *filename = getenv("OLIVE_CONFIG_FILE");
    if (filename == NULL || *filename == '\0') filename = DEFAULT_CONFIG_FILE;

    FactoryConfig config;
    if (load_config(filename, &config) != 0) {
        report_event(MSG_ERROR, 0, BATCH_ERROR, "Reception could not load factory config");
        goto done;
    }

    double batch_size = config.batch_size;
    double total;
    if (!isfinite(batch_size) || batch_size <= 0.0 ||
        get_total_olives(batch_size, &total) != 0 ||
        (total > 0.0 && total / batch_size >= INT_MAX - 1.0)) {
        fprintf(stderr, "[Reception] Invalid container quantity or batch size.\n");
        report_event(MSG_ERROR, 0, BATCH_ERROR, "Invalid Reception input quantity");
        goto done;
    }

    report_event(MSG_STAGE_STARTED, 0, BATCH_IN_PROGRESS, "Reception started");
    if (total > 0.0) {
        report_event(MSG_SENSOR_UPDATE, 0, BATCH_IN_PROGRESS,
                     "Container has olives; conveyor running");
    }

    double remaining = total;
    int id = 1;
    while (remaining > 0.0) {
        double weight = remaining < batch_size ? remaining : batch_size;
        if ((float)weight <= 0.0f) {
            report_event(MSG_ERROR, id, BATCH_ERROR, "Remaining Batch weight too small");
            break;
        }

        Batch batch = {0};
        batch.batch_id = id;
        batch.olive_weight = (float)weight;
        batch.current_stage = STAGE_RECEPTION;
        batch.status = BATCH_COMPLETED;
        initialize_batch_sensors(&batch);
        report_sensor(&batch);

        if (write_batch(write_fd, &batch) != 0) {
            fprintf(stderr, "[Reception] Cannot send Batch %d: %s\n", id, strerror(errno));
            report_event(MSG_ERROR, id, BATCH_ERROR, "Reception-to-Cleaning Pipe failed");
            goto done;
        }

        report_event(MSG_BATCH_ACCEPTED, id, BATCH_COMPLETED, "Batch accepted at Reception");
        report_event(MSG_STAGE_COMPLETED, id, BATCH_COMPLETED, "Reception Batch completed");
        printf("[Reception] Batch %d sent: %.2f kg\n", id, weight);
        fflush(stdout);

        remaining -= weight;
        if (remaining < 1e-9) remaining = 0.0;
        ++id;
    }

    report_event(MSG_SENSOR_UPDATE, 0, BATCH_COMPLETED,
                 "Container empty; conveyor stopped");
    report_event(MSG_INFO, 0, BATCH_COMPLETED,
                 "Reception finished; downstream can drain pending batches");
    printf("[Reception] Container empty. Conveyor stopped.\n");
    fflush(stdout);

done:
    if (write_fd >= 0) close(write_fd);
    if (changed) sigaction(SIGPIPE, &previous, NULL);
}

