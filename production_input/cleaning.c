#define _POSIX_C_SOURCE 200809L

#include "production_input.h"
#include "messages.h"
#include "sensors.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

// Cleaning removes light debris from incoming Batches using a simulated air blower. The removal rate belongs to THIS stage, not to Batch.

#define CLEANING_MIN_RATE 0.02f
#define CLEANING_MAX_RATE 0.05f
#define CLEANING_TIME_SECONDS 2

static void report_event(int type, int batch_id, int status, const char *text)
{
    ProcessMessage message = {0};
    message.process_id = getpid();
    message.stage_id = STAGE_CLEANING;
    message.message_type = type;
    message.batch_id = batch_id;
    message.status = status;
    snprintf(message.message, sizeof(message.message), "%s", text);

    static int warned = 0;
    int fd = open(STATUS_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        if (!warned) {
            fprintf(stderr, "[Cleaning] Supervisor FIFO unavailable; reports may be missed.\n");
            warned = 1;
        }
        return;
    }
    warned = 0;
    if (write(fd, &message, sizeof(message)) != (ssize_t)sizeof(message)) {
        fprintf(stderr, "[Cleaning] Failed to send status report.\n");
    }
    close(fd);
}

static int read_batch(int fd, Batch *batch)
{
    char *data = (char *)batch;
    size_t received = 0;
    while (received < sizeof(*batch)) {
        ssize_t n = read(fd, data + received, sizeof(*batch) - received);
        if (n > 0) received += (size_t)n;
        else if (n == 0) {
            if (received == 0) return 0;
            errno = EPROTO;
            return -1;
        } else if (errno != EINTR) return -1;
    }
    return 1;
}

static void report_sensor(const Batch *batch)
{
    if (batch == NULL) return;
    ProcessMessage message = {0};
    message.process_id = getpid();
    message.stage_id = STAGE_CLEANING;
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
        if (n > 0) written += (size_t)n;
        else if (n < 0 && errno == EINTR) continue;
        else { if (n == 0) errno = EIO; return -1; }
    }
    return 0;
}

static int valid_input(const Batch *batch)
{
    return batch->batch_id > 0 &&
           isfinite(batch->olive_weight) && batch->olive_weight > 0.0f &&
           isfinite(batch->oil_quantity) && batch->oil_quantity >= 0.0f &&
           isfinite(batch->water_quantity) && batch->water_quantity >= 0.0f &&
           isfinite(batch->pomace_quantity) && batch->pomace_quantity >= 0.0f &&
           isfinite(batch->temperature) &&
           isfinite(batch->pressure) && batch->pressure >= 0.0f &&
           batch->processing_time >= 0 &&
           batch->current_stage == STAGE_RECEPTION &&
           batch->status == BATCH_COMPLETED;
}

static void seed_random(void)
{
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
    const char *text = getenv("OLIVE_RANDOM_SEED");
    if (text != NULL && *text != '\0') {
        char *end;
        errno = 0;
        unsigned long value = strtoul(text, &end, 10);
        if (errno == 0 && end != text && *end == '\0' && value <= UINT_MAX) {
            seed = (unsigned int)value;
        }
    }
    srand(seed ^ (unsigned int)STAGE_CLEANING);
}

static float random_removal_rate(void)
{
    float fraction = (float)rand() / (float)RAND_MAX;
    return CLEANING_MIN_RATE + fraction * (CLEANING_MAX_RATE - CLEANING_MIN_RATE);
}

static void simulate_delay(int seconds)
{
    if (getenv("OLIVE_SKIP_STAGE_DELAYS") != NULL) return;
    unsigned int remaining = (unsigned int)seconds;
    while (remaining > 0) remaining = sleep(remaining);
}

void cleaning_process(int read_fd, int write_fd)
{
    struct sigaction ignore = {0}, previous;
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    int changed = (sigaction(SIGPIPE, &ignore, &previous) == 0);

    if (read_fd < 0 || write_fd < 0 || read_fd == write_fd) {
        report_event(MSG_ERROR, 0, BATCH_ERROR, "Invalid Cleaning Pipe(s)");
        goto done;
    }

    seed_random();
    for (;;) {
        Batch batch;
        int result = read_batch(read_fd, &batch);
        if (result == 0) break; 
        if (result < 0) {
            fprintf(stderr, "[Cleaning] Input Pipe failure: %s\n", strerror(errno));
            report_event(MSG_ERROR, 0, BATCH_ERROR, "Cleaning input Pipe failed");
            break;
        }
        if (!valid_input(&batch)) {
            fprintf(stderr, "[Cleaning] Rejected invalid Batch %d.\n", batch.batch_id);
            report_event(MSG_BATCH_REJECTED, batch.batch_id, BATCH_REJECTED,
                         "Cleaning rejected invalid Batch");
            continue;
        }

        report_event(MSG_STAGE_STARTED, batch.batch_id, BATCH_IN_PROGRESS,
                     "Cleaning started");
        batch.status = BATCH_IN_PROGRESS;
        simulate_delay(CLEANING_TIME_SECONDS);

        float before = batch.olive_weight;
        float rate = random_removal_rate();
        float removed = before * rate;
        float after = before - removed;
        if (!isfinite(after) || after <= 0.0f ||
            batch.processing_time > INT_MAX - CLEANING_TIME_SECONDS) {
            report_event(MSG_BATCH_REJECTED, batch.batch_id, BATCH_REJECTED,
                         "Cleaning rejected unusable Batch");
            continue;
        }

        batch.olive_weight = after;
        batch.processing_time += CLEANING_TIME_SECONDS;
        update_stage_sensors(&batch, STAGE_CLEANING, NULL);
        report_sensor(&batch);
        batch.current_stage = STAGE_CLEANING;
        batch.status = BATCH_COMPLETED;

        if (write_batch(write_fd, &batch) != 0) {
            fprintf(stderr, "[Cleaning] Output Pipe failure: %s\n", strerror(errno));
            report_event(MSG_ERROR, batch.batch_id, BATCH_ERROR,
                         "Cleaning-to-Washing Pipe failed");
            break;
        }

        char description[MAX_MESSAGE];
        snprintf(description, sizeof(description),
                 "Cleaning done: rate %.2f%%, removed %.2f kg, left %.2f kg",
                 rate * 100.0f, removed, after);
        report_event(MSG_STAGE_COMPLETED, batch.batch_id, BATCH_COMPLETED, description);
        printf("[Cleaning] Batch %d: %.2f -> %.2f kg; removed %.2f kg (%.2f%%); time +%d s\n",
               batch.batch_id, before, after, removed, rate * 100.0f,
               CLEANING_TIME_SECONDS);
        fflush(stdout);
    }

    report_event(MSG_INFO, 0, BATCH_COMPLETED, "Cleaning stopped after EOF");

done:
    if (read_fd >= 0) close(read_fd);
    if (write_fd >= 0 && write_fd != read_fd) close(write_fd);
    if (changed) sigaction(SIGPIPE, &previous, NULL);
}

