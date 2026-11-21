#ifndef QUALITY_CONTROL_H
#define QUALITY_CONTROL_H

#include "project_contract.h"

typedef enum {
    QUALITY_REJECTED = 0,
    QUALITY_ACCEPTED = 1
} QualityResult;

int calculate_quality_score(const Batch *batch);

QualityResult check_batch_quality(
    const Batch *batch,
    int min_quality_score,
    float max_temperature,
    float max_pressure
);

void print_quality_report(const Batch *batch, int quality_score);

#endif
