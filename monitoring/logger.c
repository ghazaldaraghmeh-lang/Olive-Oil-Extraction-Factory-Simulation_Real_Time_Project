/* Centralized event logging implementation. */

#include <stdio.h>
#include <time.h>
#include "logger.h"

static FILE *log_file = NULL;

int logger_init(void)
{
    log_file = fopen("factory.log", "a");

    if (log_file == NULL) {
        perror("Error opening log file");
        return -1;
    }

    return 0;
}

void log_info(const char *message)
{
    if (log_file == NULL || message == NULL) {
        return;
    }

    time_t current_time = time(NULL);
    struct tm *time_info = localtime(&current_time);

    if (time_info == NULL) {
        return;
    }

    char timestamp[64];

    strftime(timestamp, sizeof(timestamp),
             "%Y-%m-%d %H:%M:%S", time_info);

    fprintf(log_file, "[%s] [INFO] %s\n",
            timestamp, message);

    fflush(log_file);
}

void log_error(const char *message)
{
    if (log_file == NULL || message == NULL) {
        return;
    }

    time_t current_time = time(NULL);
    struct tm *time_info = localtime(&current_time);

    if (time_info == NULL) {
        return;
    }

    char timestamp[64];

    strftime(timestamp, sizeof(timestamp),
             "%Y-%m-%d %H:%M:%S", time_info);

    fprintf(log_file, "[%s] [ERROR] %s\n",
            timestamp, message);

    fflush(log_file);
}


void log_process_message(const ProcessMessage *msg)
{
    if (log_file == NULL || msg == NULL) {
        return;
    }

    time_t current_time = time(NULL);
    struct tm *time_info = localtime(&current_time);

    if (time_info == NULL) {
        return;
    }

    char timestamp[64];

    strftime(timestamp, sizeof(timestamp),
             "%Y-%m-%d %H:%M:%S", time_info);

    const char *type = "UNKNOWN";

    switch (msg->message_type) {
        case MSG_STAGE_STARTED:
            type = "STAGE_STARTED";
            break;
        case MSG_STAGE_COMPLETED:
            type = "STAGE_COMPLETED";
            break;
        case MSG_SENSOR_UPDATE:
            type = "SENSOR_UPDATE";
            break;
        case MSG_ERROR:
            type = "ERROR";
            break;
        case MSG_WARNING:
            type = "WARNING";
            break;
        case MSG_BATCH_ACCEPTED:
            type = "BATCH_ACCEPTED";
            break;
        case MSG_BATCH_REJECTED:
            type = "BATCH_REJECTED";
            break;
        case MSG_FACTORY_STOP:
            type = "FACTORY_STOP";
            break;
        case MSG_FACTORY_SHUTDOWN:
            type = "FACTORY_SHUTDOWN";
            break;
        case MSG_INFO:
            type = "INFO";
            break;
    }

    fprintf(log_file,
            "[%s] [%s] PID=%ld Stage=%d Batch=%d "
            "Temp=%.2f Pressure=%.2f Status=%d Message=%.*s\n",
            timestamp,
            type,
            (long)msg->process_id,
            msg->stage_id,
            msg->batch_id,
            msg->temperature,
            msg->pressure,
            msg->status,
            (int)sizeof(msg->message),
            msg->message);

    fflush(log_file);
}


void logger_close(void)
{
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}
