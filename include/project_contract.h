#ifndef PROJECT_CONTRACT_H
#define PROJECT_CONTRACT_H

#include <sys/types.h>

/* ENCS4330 Olive Oil Factory - Shared Contract */

#define NUM_STAGES 8
#define NUM_PRODUCTION_STAGES 6
#define NUM_PIPES 5
#define MAX_MESSAGE 128
#define DEFAULT_CONFIG_FILE "config.txt"
#define STATUS_FIFO "/tmp/olive_factory_status"
#define DISPLAY_FIFO "/tmp/olive_factory_display"

typedef enum {
    STAGE_RECEPTION = 0,
    STAGE_CLEANING,
    STAGE_WASHING,
    STAGE_CRUSHING,
    STAGE_MALAXATION,
    STAGE_SEPARATION,
    STAGE_QUALITY_CONTROL,
    STAGE_SUPERVISOR
} StageID;

typedef enum {
    BATCH_PENDING = 0,
    BATCH_IN_PROGRESS,
    BATCH_COMPLETED,
    BATCH_REJECTED,
    BATCH_ERROR
} BatchStatus;

typedef struct {
    int batch_id;
    float olive_weight;
    float oil_quantity;
    float water_quantity;
    float pomace_quantity;
    float temperature;
    float pressure;
    int processing_time;
    int current_stage;
    int status;
} Batch;

typedef struct {
    StageID stage;
    pid_t pid;
} ProcessInfo;

#endif
