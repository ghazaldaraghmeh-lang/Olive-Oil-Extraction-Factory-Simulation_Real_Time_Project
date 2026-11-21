
#include <stdio.h>
#include "quality_control.h"

#include <math.h>

int calculate_quality_score(const Batch *batch)
{
    if (batch == NULL) {
        return 0;
    }

    if (!isfinite(batch->olive_weight) ||
        !isfinite(batch->oil_quantity) ||
        !isfinite(batch->water_quantity) ||
        !isfinite(batch->pomace_quantity) ||
        batch->olive_weight <= 0 ||
        batch->oil_quantity < 0 ||
        batch->water_quantity < 0 ||
        batch->pomace_quantity < 0) {
        return 0;
    }

    float output = batch->oil_quantity +
                   batch->water_quantity +
                   batch->pomace_quantity;

    float difference = fabsf(batch->olive_weight - output);

    float score = 100.0f -
                  (difference / batch->olive_weight) * 100.0f;

    if (score < 0.0f) {
        score = 0.0f;
    }

    if (score > 100.0f) {
        score = 100.0f;
    }

    return (int)roundf(score);
}

QualityResult check_batch_quality(
    const Batch *batch,
    int min_quality_score,
    float max_temperature,
    float max_pressure)
{
    if (batch == NULL) {
        return QUALITY_REJECTED;
    }

    if (!isfinite(batch->temperature) ||
        !isfinite(batch->pressure) ||
        !isfinite(max_temperature) ||
        !isfinite(max_pressure)) {
        return QUALITY_REJECTED;
    }

    if (min_quality_score < 0 || min_quality_score > 100 ||
        max_temperature <= 0 || max_pressure <= 0) {
        return QUALITY_REJECTED;
    }

    if (batch->temperature < 0 ||
        batch->temperature > max_temperature) {
        return QUALITY_REJECTED;
    }

    if (batch->pressure < 0 ||
        batch->pressure > max_pressure) {
        return QUALITY_REJECTED;
    }

    int score = calculate_quality_score(batch);

    if (score < min_quality_score) {
        return QUALITY_REJECTED;
    }

    return QUALITY_ACCEPTED;
}



void print_quality_report(const Batch *batch, int quality_score)
{
    if (batch == NULL) {
        printf("Error: Invalid batch.\n");
        return;
    }

    printf("\n========== QUALITY REPORT ==========\n");

    printf("Olive Weight    : %.2f kg\n", batch->olive_weight);
    printf("Oil Quantity    : %.2f\n", batch->oil_quantity);
    printf("Water Quantity  : %.2f\n", batch->water_quantity);
    printf("Pomace Quantity : %.2f\n", batch->pomace_quantity);

    printf("Temperature     : %.2f\n", batch->temperature);
    printf("Pressure        : %.2f\n", batch->pressure);
    printf("Processing Time : %d\n", batch->processing_time);

    printf("Quality Score   : %d / 100\n", quality_score);

    printf("====================================\n");
}
