
/* Pressing and separation stage implementation. */
#define _POSIX_C_SOURCE 200809L

#include "../include/production_output.h"
#include "../include/config.h"
#include "../include/messages.h"
#include "parallel_processing.h"
#include "sensors.h"

#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define OPENMP_TIMING_FILE "/tmp/olive_factory_openmp_timing"

static int read_batch(int fd, Batch *batch)
{
    unsigned char *p = (unsigned char *)batch;
    size_t read_count = 0;

    while (read_count < sizeof(*batch)) {
        ssize_t n = read(fd, p + read_count,
                         sizeof(*batch) - read_count);

        if (n > 0)
            read_count += (size_t)n;
        else if (n == 0) {
            if (read_count == 0)
                return 0;

            fprintf(stderr, "[Separation] Incomplete Batch.\n");
            return -1;
        }
        else if (errno != EINTR) {
            perror("[Separation] read");
            return -1;
        }
    }

    return 1;
}

static int write_batch(int fd, const Batch *batch)
{
    const unsigned char *p = (const unsigned char *)batch;
    size_t count = 0;

    while (count < sizeof(*batch)) {
        ssize_t n = write(fd, p + count,
                          sizeof(*batch) - count);

        if (n > 0)
            count += (size_t)n;
        else if (n < 0 && errno == EINTR)
            continue;
        else {
            if (n < 0)
                perror("[Separation] write");
            else
                fprintf(stderr,
                        "[Separation] Cannot forward Batch.\n");

            return -1;
        }
    }

    return 0;
}

static void report_to_supervisor(int message_type,
                                 const Batch *batch,
                                 const char *detail)
{
    ProcessMessage msg = {0};

    msg.process_id = getpid();
    msg.stage_id = STAGE_SEPARATION;
    msg.message_type = message_type;
    msg.batch_id = batch ? batch->batch_id : -1;
    msg.temperature = batch ? batch->temperature : 0.0f;
    msg.pressure = batch ? batch->pressure : 0.0f;
    msg.status = batch ? batch->status : BATCH_ERROR;

    snprintf(msg.message, sizeof(msg.message), "%s", detail);

    int fifo = open(STATUS_FIFO, O_WRONLY | O_NONBLOCK);

    if (fifo < 0) {
        if (errno != ENOENT && errno != ENXIO)
            perror("[Separation] open Supervisor FIFO");

        return;
    }

    ssize_t n;

    do {
        n = write(fifo, &msg, sizeof(msg));
    } while (n < 0 && errno == EINTR);

    if (n != (ssize_t)sizeof(msg)) {
        if (n < 0)
            perror("[Separation] Supervisor FIFO write");
        else
            fprintf(stderr,
                    "[Separation] Incomplete Supervisor report.\n");
    }

    close(fifo);
}

typedef struct {
    double water_share;
    int portions;
    int use_openmp;
} SeparationOptions;

static int load_optional_separation_options(
    const char *filename,
    SeparationOptions *options)
{
    options->water_share = 0.60;
    options->portions = 100000;
    options->use_openmp = 1;

    FILE *f = fopen(filename, "r");

    if (!f)
        return 0;

    char line[MAX_CONFIG_LINE];

    while (fgets(line, sizeof(line), f)) {
        char key[80];
        double val;

        if (sscanf(line, " %79[^= \t] = %lf",
                   key, &val) != 2)
            continue;

        if (strcmp(key, "SEPARATION_WATER_SHARE") == 0) {
            options->water_share = val;
        }
        else if (strcmp(key, "SEPARATION_PORTIONS") == 0) {
            if (!isfinite(val) || val < 1 ||
                val > 100000000 || floor(val) != val) {
                fclose(f);
                return -1;
            }

            options->portions = (int)val;
        }
        else if (strcmp(key, "SEPARATION_USE_OPENMP") == 0) {
            if (val != 0.0 && val != 1.0) {
                fclose(f);
                return -1;
            }

            options->use_openmp = (int)val;
        }
    }

    fclose(f);

    return isfinite(options->water_share) &&
           options->water_share >= 0.0 &&
           options->water_share <= 1.0 ? 0 : -1;
}

static int get_separation_config(
    FactoryConfig *config,
    SeparationOptions *options)
{
    set_default_config(config);

    const char *filename = getenv("OLIVE_CONFIG_FILE");

    if (!filename || !filename[0])
        filename = DEFAULT_CONFIG_FILE;

    if (load_config(filename, config) != 0)
        fprintf(stderr,
                "[Separation] Cannot load '%s'; using defaults.\n",
                filename);

    if (load_optional_separation_options(filename, options) != 0)
        return -1;

    if (config->separation_time < 0 ||
        !isfinite(config->oil_extraction_rate) ||
        config->oil_extraction_rate < 0.0f ||
        config->oil_extraction_rate > 1.0f ||
        !isfinite(config->max_temperature) ||
        config->max_temperature <= 0.0f ||
        !isfinite(config->max_pressure) ||
        config->max_pressure <= 0.0f)
        return -1;

    return 0;
}

/*
 * Save the average yield-calculation time.
 * Main reads this file after production processes finish.
 */
static void save_openmp_timing(double total_time, int count)
{
    FILE *fp = fopen(OPENMP_TIMING_FILE, "w");

    if (!fp) {
        perror("[Separation] Cannot save OpenMP timing");
        return;
    }

    double average = count > 0 ? total_time / count : 0.0;

    fprintf(fp, "%.9f %d\n", average, count);

    fclose(fp);
}

void separation_process(int read_fd, int write_fd)
{
    FactoryConfig config;
    SeparationOptions options;
    Batch batch;

    int result = 0;

    /* OpenMP timing statistics */
    double total_calculation_time = 0.0;
    int calculation_count = 0;

    struct sigaction ignore_sigpipe = {0}, old_sigpipe;
    int have_old_sigpipe = 0;

    ignore_sigpipe.sa_handler = SIG_IGN;
    sigemptyset(&ignore_sigpipe.sa_mask);

    if (sigaction(SIGPIPE, &ignore_sigpipe, &old_sigpipe) == 0)
        have_old_sigpipe = 1;

    if (get_separation_config(&config, &options) != 0) {
        report_to_supervisor(
            MSG_ERROR, NULL,
            "Separation configuration invalid");

        goto cleanup;
    }

    while ((result = read_batch(read_fd, &batch)) == 1) {

        if (batch.status == BATCH_ERROR ||
            batch.status == BATCH_REJECTED) {

            report_to_supervisor(
                MSG_WARNING,
                &batch,
                "Earlier stage failed; separation skipped");

        }
        else {
            batch.current_stage = STAGE_SEPARATION;
            batch.status = BATCH_IN_PROGRESS;

            const char *why = NULL;

            if (!isfinite(batch.olive_weight) ||
                batch.olive_weight <= 0.0f)
                why = "Invalid olive weight";

            else if (!isfinite(batch.temperature) ||
                     !isfinite(batch.pressure) ||
                     batch.temperature > config.max_temperature ||
                     batch.pressure > config.max_pressure)
                why = "Temperature/pressure outside limits";

            else if (batch.processing_time < 0 ||
                     batch.processing_time >
                     INT_MAX - config.separation_time)
                why = "Invalid/overflowing processing time";

            if (why) {
                batch.status = BATCH_ERROR;

                report_to_supervisor(
                    MSG_ERROR, &batch, why);
            }
            else {
                double oil, water, pomace;

                struct timespec t0, t1;

                clock_gettime(CLOCK_MONOTONIC, &t0);

                int calculation_result =
                    calculate_separation_yields(
                        batch.olive_weight,
                        config.oil_extraction_rate,
                        options.water_share,
                        options.portions,
                        options.use_openmp,
                        &oil,
                        &water,
                        &pomace);

                clock_gettime(CLOCK_MONOTONIC, &t1);

                if (calculation_result != 0 ||
                    oil > FLT_MAX ||
                    water > FLT_MAX ||
                    pomace > FLT_MAX ||
                    oil < 0.0 ||
                    water < 0.0 ||
                    pomace < 0.0) {

                    batch.status = BATCH_ERROR;

                    report_to_supervisor(
                        MSG_ERROR,
                        &batch,
                        "Yield computation failed");
                }
                else {
                    double elapsed =
                        (double)(t1.tv_sec - t0.tv_sec) +
                        (double)(t1.tv_nsec - t0.tv_nsec) /
                        1000000000.0;

                    /* Update OpenMP timing statistics */
                    total_calculation_time += elapsed;
                    calculation_count++;

                    printf(
                        "[Separation] Yield calculation: %s | "
                        "portions=%d | elapsed=%.6f s\n",
                        options.use_openmp
                            ? "OpenMP ON"
                            : "OpenMP OFF",
                        options.portions,
                        elapsed);

                    fflush(stdout);

                    report_to_supervisor(
                        MSG_STAGE_STARTED,
                        &batch,
                        "Separation started");

                    unsigned int remaining =
                        (unsigned int)config.separation_time;

                    while (remaining > 0)
                        remaining = sleep(remaining);

                    batch.oil_quantity = (float)oil;
                    batch.water_quantity = (float)water;
                    batch.pomace_quantity = (float)pomace;

                    batch.processing_time +=
                        config.separation_time;

                    update_stage_sensors(
                        &batch,
                        STAGE_SEPARATION,
                        &config);

                    batch.status = BATCH_IN_PROGRESS;

                    char report[MAX_MESSAGE];

                    snprintf(
                        report,
                        sizeof(report),
                        "Separation completed: oil %.2f kg, "
                        "water %.2f kg, pomace %.2f kg",
                        oil,
                        water,
                        pomace);

                    report_to_supervisor(
                        MSG_STAGE_COMPLETED,
                        &batch,
                        report);

                    printf(
                        "[Separation] Batch %d: oil %.2f, "
                        "water %.2f, pomace %.2f kg\n",
                        batch.batch_id,
                        oil,
                        water,
                        pomace);

                    fflush(stdout);
                }
            }
        }

        if (write_batch(write_fd, &batch) != 0) {
            report_to_supervisor(
                MSG_ERROR,
                &batch,
                "Could not forward to Quality Control");

            break;
        }
    }

    if (result < 0) {
        report_to_supervisor(
            MSG_ERROR,
            NULL,
            "Separation input pipe read failed");
    }

cleanup:

    /* Save average calculation time for main.c */
    save_openmp_timing(
        total_calculation_time,
        calculation_count);

    close(read_fd);
    close(write_fd);

    if (have_old_sigpipe)
        sigaction(SIGPIPE, &old_sigpipe, NULL);
}
