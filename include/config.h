#ifndef CONFIG_H
#define CONFIG_H

#define MAX_CONFIG_LINE 256
#define TEST_REJECT_NONE 0
#define TEST_REJECT_TEMPERATURE 1
#define TEST_REJECT_PRESSURE 2

/*
Example config.txt:
BATCH_SIZE=120
MAX_TEMPERATURE=35
MAX_PRESSURE=50
WASHING_TIME=5
CRUSHING_TIME=10
MALAXATION_TIME=8
SEPARATION_TIME=10
MIN_QUALITY_SCORE=60
OIL_EXTRACTION_RATE=0.20
SEPARATION_WATER_SHARE=0.60
SEPARATION_PORTIONS=100000
SEPARATION_USE_OPENMP=1
*/

typedef struct {
    float batch_size;
    float max_temperature;
    float max_pressure;
    int washing_time;
    int crushing_time;
    int malaxation_time;
    int separation_time;
    int min_quality_score;
    float oil_extraction_rate;
    float separation_water_share;
    int separation_portions;
    int separation_use_openmp;
int test_reject_batch;
int test_reject_reason;
} FactoryConfig;

int load_config(const char *filename, FactoryConfig *config);
void set_default_config(FactoryConfig *config);
void print_config(const FactoryConfig *config);

#endif
