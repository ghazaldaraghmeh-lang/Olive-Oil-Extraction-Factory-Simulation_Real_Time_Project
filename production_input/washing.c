#define _POSIX_C_SOURCE 200809L

#include "production_input.h"
#include "messages.h"
#include "config.h"

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

// Washing receives cleaned Batches, removes remaining surface dirt, and forwards them to Crushing.

#define WASHING_MIN_RATE 0.005f
#define WASHING_MAX_RATE 0.02f

static void report_event(int type, int batch_id, int status, const char *text)
{
    ProcessMessage message = {0};
    message.process_id = getpid();
    message.stage_id = STAGE_WASHING;
    message.message_type = type;
    message.batch_id = batch_id;
    message.status = status;
    snprintf(message.message, sizeof(message.message), "%s", text);

    static int warned = 0;
    int fd = open(STATUS_FIFO, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        if (!warned) {
            fprintf(stderr, "[Washing] Supervisor FIFO unavailable; reports may be missed.\n");
            warned = 1;
        }
        return;
    }
    warned = 0;
    if (write(fd, &message, sizeof(message)) != (ssize_t)sizeof(message)) {
        fprintf(stderr, "[Washing] Failed to send status report.\n");
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
           batch->current_stage == STAGE_CLEANING &&
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
    srand(seed ^ (unsigned int)STAGE_WASHING);
}

static float random_removal_rate(void)
{
    float fraction = (float)rand() / (float)RAND_MAX;
    return WASHING_MIN_RATE + fraction * (WASHING_MAX_RATE - WASHING_MIN_RATE);
}

static void simulate_delay(int seconds)
{
    if (getenv("OLIVE_SKIP_STAGE_DELAYS") != NULL) return;
    unsigned int remaining = (unsigned int)seconds;
    while (remaining > 0) remaining = sleep(remaining);
}

void washing_process(int read_fd, int write_fd)
{
    struct sigaction ignore = {0}, previous;
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    int changed = (sigaction(SIGPIPE, &ignore, &previous) == 0);

    if (read_fd < 0 || write_fd < 0 || read_fd == write_fd) {
        report_event(MSG_ERROR, 0, BATCH_ERROR, "Invalid Washing Pipe(s)");
        goto done;
    }

    const char *filename = getenv("OLIVE_CONFIG_FILE");
    if (filename == NULL || *filename == '\0') filename = DEFAULT_CONFIG_FILE;

    FactoryConfig config;
    if (load_config(filename, &config) != 0) {
        report_event(MSG_ERROR, 0, BATCH_ERROR, "Washing could not load factory config");
        goto done;
    }
    if (config.washing_time < 0) {
        report_event(MSG_ERROR, 0, BATCH_ERROR, "Invalid washing time");
        goto done;
    }

    seed_random();
    for (;;) {
        Batch batch;
        int result = read_batch(read_fd, &batch);
        if (result == 0) break;
        if (result < 0) {
            fprintf(stderr, "[Washing] Input Pipe failure: %s\n", strerror(errno));
            report_event(MSG_ERROR, 0, BATCH_ERROR, "Washing input Pipe failed");
            break;
        }
        if (!valid_input(&batch)) {
            fprintf(stderr, "[Washing] Rejected invalid Batch %d.\n", batch.batch_id);
            report_event(MSG_BATCH_REJECTED, batch.batch_id, BATCH_REJECTED,
                         "Washing rejected invalid Batch");
            continue;
        }

        report_event(MSG_STAGE_STARTED, batch.batch_id, BATCH_IN_PROGRESS,
                     "Washing started");
        batch.status = BATCH_IN_PROGRESS;
        simulate_delay(config.washing_time);

        float before = batch.olive_weight;
        float rate = random_removal_rate();
        float removed = before * rate;
        float after = before - removed;
        if (!isfinite(after) || after <= 0.0f ||
            batch.processing_time > INT_MAX - config.washing_time) {
            report_event(MSG_BATCH_REJECTED, batch.batch_id, BATCH_REJECTED,
                         "Washing rejected unusable Batch");
            continue;
        }

        batch.olive_weight = after;
        batch.processing_time += config.washing_time;
        batch.current_stage = STAGE_WASHING;
        batch.status = BATCH_COMPLETED;

        if (write_batch(write_fd, &batch) != 0) {
            fprintf(stderr, "[Washing] Output Pipe failure: %s\n", strerror(errno));
            report_event(MSG_ERROR, batch.batch_id, BATCH_ERROR,
                         "Washing-to-Crushing Pipe failed");
            break;
        }

        char description[MAX_MESSAGE];
        snprintf(description, sizeof(description),
                 "Washing done: rate %.2f%%, removed %.2f kg, left %.2f kg",
                 rate * 100.0f, removed, after);
        report_event(MSG_STAGE_COMPLETED, batch.batch_id, BATCH_COMPLETED, description);
        printf("[Washing] Batch %d: %.2f -> %.2f kg; removed %.2f kg (%.2f%%); time +%d s\n",
               batch.batch_id, before, after, removed, rate * 100.0f,
               config.washing_time);
        fflush(stdout);
    }

    report_event(MSG_INFO, 0, BATCH_COMPLETED, "Washing stopped after EOF");

done:
    if (read_fd >= 0) close(read_fd);
    if (write_fd >= 0 && write_fd != read_fd) close(write_fd);
    if (changed) sigaction(SIGPIPE, &previous, NULL);
}
