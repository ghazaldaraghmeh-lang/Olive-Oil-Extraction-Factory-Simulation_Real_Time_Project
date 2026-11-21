
#ifndef DISPLAY_H
#define DISPLAY_H
#include "project_contract.h"
#include "messages.h"

/* Factory stage status */
typedef enum {
    STAGE_IDLE = 0,
    STAGE_RUNNING,
    STAGE_COMPLETED,
    STAGE_ERROR
} StageStatus;

/* Factory production statistics */
typedef struct {
    int total_batches;
    int accepted_batches;
    int rejected_batches;

    float total_oil;
    float total_water;
    float total_pomace;
} FactoryStatistics;

/* Factory sensor readings */
typedef struct {
    float temperature;
    float pressure;
    int alarm_active;
} FactorySensors;

/* Initialize and run the GUI */
void display_init(int argc, char **argv);
void display_run(void);
void display_close(void);

/* Update factory statistics */
void display_update_statistics(
    const FactoryStatistics *stats
);

/* Update sensor readings */
void display_update_sensors(
    const FactorySensors *sensors
);

/* Update one factory stage status */
void display_update_stage(
    StageID stage_id,
    StageStatus status
);


/* Update GUI using a factory process message */
void display_process_message(
    const ProcessMessage *msg
);


/* Display factory alerts and notifications */
typedef enum {
    ALERT_NONE = 0,
    ALERT_INFO,
    ALERT_WARNING,
    ALERT_ERROR
} AlertType;

void display_update_alert(
    AlertType type,
    const char *message
);


#endif
