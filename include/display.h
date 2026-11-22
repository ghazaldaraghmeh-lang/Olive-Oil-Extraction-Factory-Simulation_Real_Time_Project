#ifndef DISPLAY_H
#define DISPLAY_H

#include "project_contract.h"
#include "messages.h"

typedef enum {
    STAGE_IDLE = 0,
    STAGE_RUNNING,
    STAGE_COMPLETED,
    STAGE_ERROR
} StageStatus;

typedef struct {
    int total_batches;
    int accepted_batches;
    int rejected_batches;
    float total_oil;
    float total_water;
    float total_pomace;
} FactoryStatistics;

typedef struct {
    float temperature;
    float pressure;
    int alarm_active;
} FactorySensors;

void display_init(int argc, char **argv);
void display_run(void);
void display_close(void);
void display_update_statistics(const FactoryStatistics *stats);
void display_update_sensors(const FactorySensors *sensors);
void display_update_stage(StageID stage_id, StageStatus status);
void display_process_message(const ProcessMessage *msg);

typedef enum {
    ALERT_NONE = 0,
    ALERT_INFO,
    ALERT_WARNING,
    ALERT_ERROR
} AlertType;

void display_update_alert(AlertType type, const char *message);

#endif
