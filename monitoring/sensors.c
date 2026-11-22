#include "sensors.h"

#include <math.h>
#include <stddef.h>

static float clamp_float(float value, float maximum, float margin)
{
    if (maximum <= 0.0f) return value;
    float limit = maximum - margin;
    if (limit < 0.0f) limit = maximum;
    return value > limit ? limit : value;
}

void initialize_batch_sensors(Batch *batch)
{
    if (batch == NULL) return;
    batch->temperature = 24.0f;
    batch->pressure = 8.0f;
}

void update_stage_sensors(Batch *batch, StageID stage, const FactoryConfig *config)
{
    if (batch == NULL) return;

    float max_t = config ? config->max_temperature : 35.0f;
    float max_p = config ? config->max_pressure : 50.0f;

    switch (stage) {
        case STAGE_RECEPTION:
            batch->temperature += 0.5f;
            batch->pressure += 1.0f;
            break;
        case STAGE_CLEANING:
            batch->temperature += 1.0f;
            batch->pressure += 2.0f;
            break;
        case STAGE_WASHING:
            batch->temperature += 2.0f;
            batch->pressure += 3.0f;
            break;
        case STAGE_CRUSHING:
            batch->temperature += 3.0f;
            batch->pressure += 7.0f;
            break;
        case STAGE_MALAXATION:
            batch->temperature += 2.0f;
            batch->pressure += 5.0f;
            break;
        case STAGE_SEPARATION:
            batch->temperature += 1.0f;
            batch->pressure += 6.0f;
            break;
        default:
            break;
    }

  batch->temperature = clamp_float(batch->temperature, max_t, 1.0f);
batch->pressure = clamp_float(batch->pressure, max_p, 1.0f);

if (!isfinite(batch->temperature)) batch->temperature = 24.0f;
if (!isfinite(batch->pressure)) batch->pressure = 8.0f;

/* Optional test mode: reject a selected batch */
if (config != NULL &&
    stage == STAGE_SEPARATION &&
    batch->batch_id == config->test_reject_batch) {

    if (config->test_reject_reason == TEST_REJECT_TEMPERATURE) {
        batch->temperature = max_t + 5.0f;
    }
    else if (config->test_reject_reason == TEST_REJECT_PRESSURE) {
        batch->pressure = max_p + 5.0f;
    }
}


}
