#ifndef MESSAGES_H
#define MESSAGES_H

#include "project_contract.h"

typedef enum {
    MSG_STAGE_STARTED = 1,
    MSG_STAGE_COMPLETED,
    MSG_SENSOR_UPDATE,
    MSG_ERROR,
    MSG_WARNING,
    MSG_BATCH_ACCEPTED,
    MSG_BATCH_REJECTED,
    MSG_FACTORY_STOP,
    MSG_FACTORY_SHUTDOWN,
    MSG_INFO
} MessageType;

typedef struct {
    pid_t process_id;
    int stage_id;
    int message_type;
    int batch_id;
    float temperature;
    float pressure;
    int status;
    char message[MAX_MESSAGE];
} ProcessMessage;

#endif
