#ifndef SENSORS_H
#define SENSORS_H

#include "project_contract.h"
#include "config.h"

void initialize_batch_sensors(Batch *batch);
void update_stage_sensors(Batch *batch, StageID stage, const FactoryConfig *config);

#endif
