/* Configuration file parsing implementation. */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

#include "config.h"

void set_default_config(FactoryConfig *config)
{
    if (config == NULL) {
        return;
    }

    config->batch_size = 120;
    config->max_temperature = 35;
    config->max_pressure = 50;
    config->washing_time = 5;
    config->crushing_time = 10;
    config->malaxation_time = 8;
    config->separation_time = 10;
    config->min_quality_score = 60;
    config->oil_extraction_rate = 0.20f;
    config->separation_water_share = 0.60f;
    config->separation_portions = 100000;
    config->separation_use_openmp = 1;
config->test_reject_batch = 0;
config->test_reject_reason = TEST_REJECT_NONE;
}



static int parse_float(const char *text, float *result)
{
    char *end;
    errno = 0;

    float value = strtof(text, &end);

    if (text == end || errno == ERANGE || !isfinite(value))
        return -1;

    while (isspace((unsigned char)*end))
        end++;

    if (*end != '\0')
        return -1;

    *result = value;
    return 0;
}

static int parse_integer(const char *text, int *result)
{
    char *end;
    errno = 0;

    long value = strtol(text, &end, 10);

    if (text == end || errno == ERANGE ||
        value < INT_MIN || value > INT_MAX)
        return -1;

    while (isspace((unsigned char)*end))
        end++;

    if (*end != '\0')
        return -1;

    *result = (int)value;
    return 0;
}

int load_config(const char *filename, FactoryConfig *config)
{
    if (filename == NULL || config == NULL)
        return -1;

    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        perror("Error opening configuration file");
        return -1;
    }

    FactoryConfig temp;
    set_default_config(&temp);

    char line[MAX_CONFIG_LINE];
    int line_number = 0;
    int seen[14] = {0};

    while (fgets(line, sizeof(line), file) != NULL) {
        line_number++;

        // Reject lines that are too long
        if (strchr(line, '\n') == NULL && !feof(file)) {
            int ch;
            while ((ch = fgetc(file)) != '\n' && ch != EOF)
                ;
            fprintf(stderr, "Line %d is too long\n", line_number);
            fclose(file);
            return -1;
        }

        // Ignore empty lines and comments
        char *start = line;

        while (isspace((unsigned char)*start))
            start++;

        if (*start == '\0' || *start == '#')
            continue;

        // Separate KEY and VALUE
        char *equal = strchr(start, '=');

        if (equal == NULL) {
            fprintf(stderr, "Invalid format at line %d\n", line_number);
            fclose(file);
            return -1;
        }

        *equal = '\0';
        char *key = start;
        char *value = equal + 1;

        // Remove spaces around key and value
        char *end = key + strlen(key);

        while (end > key && isspace((unsigned char)end[-1]))
            *--end = '\0';

        while (isspace((unsigned char)*value))
            value++;

        end = value + strlen(value);

        while (end > value && isspace((unsigned char)end[-1]))
            *--end = '\0';

        int index = -1;
        int valid = -1;

        if (strcmp(key, "BATCH_SIZE") == 0) {
            index = 0;
            valid = parse_float(value, &temp.batch_size);
        }
        else if (strcmp(key, "MAX_TEMPERATURE") == 0) {
            index = 1;
            valid = parse_float(value, &temp.max_temperature);
        }
        else if (strcmp(key, "MAX_PRESSURE") == 0) {
            index = 2;
            valid = parse_float(value, &temp.max_pressure);
        }
        else if (strcmp(key, "WASHING_TIME") == 0) {
            index = 3;
            valid = parse_integer(value, &temp.washing_time);
        }
        else if (strcmp(key, "CRUSHING_TIME") == 0) {
            index = 4;
            valid = parse_integer(value, &temp.crushing_time);
        }
        else if (strcmp(key, "MALAXATION_TIME") == 0) {
            index = 5;
            valid = parse_integer(value, &temp.malaxation_time);
        }
        else if (strcmp(key, "SEPARATION_TIME") == 0) {
            index = 6;
            valid = parse_integer(value, &temp.separation_time);
        }
        else if (strcmp(key, "MIN_QUALITY_SCORE") == 0) {
            index = 7;
            valid = parse_integer(value, &temp.min_quality_score);
        }
        else if (strcmp(key, "OIL_EXTRACTION_RATE") == 0) {
            index = 8;
            valid = parse_float(value, &temp.oil_extraction_rate);
        }
        else if (strcmp(key, "SEPARATION_WATER_SHARE") == 0) {
            index = 9;
            valid = parse_float(value, &temp.separation_water_share);
        }
        else if (strcmp(key, "SEPARATION_PORTIONS") == 0) {
            index = 10;
            valid = parse_integer(value, &temp.separation_portions);
        }
        else if (strcmp(key, "SEPARATION_USE_OPENMP") == 0) {
            index = 11;
            valid = parse_integer(value, &temp.separation_use_openmp);
        }
else if (strcmp(key, "TEST_REJECT_BATCH") == 0) {
    index = 12;
    valid = parse_integer(value, &temp.test_reject_batch);
}
else if (strcmp(key, "TEST_REJECT_REASON") == 0) {
    index = 13;

    if (strcmp(value, "NONE") == 0) {
        temp.test_reject_reason = TEST_REJECT_NONE;
        valid = 0;
    }
    else if (strcmp(value, "TEMPERATURE") == 0) {
        temp.test_reject_reason = TEST_REJECT_TEMPERATURE;
        valid = 0;
    }
    else if (strcmp(value, "PRESSURE") == 0) {
        temp.test_reject_reason = TEST_REJECT_PRESSURE;
        valid = 0;
    }
}

        if (index == -1 || valid != 0 ||
            (index >= 0 && seen[index])) {
            fprintf(stderr, "Invalid config at line %d: %s\n",
                    line_number, key);
            fclose(file);
            return -1;
        }

        seen[index] = 1;
    }

    if (ferror(file)) {
        perror("Error reading configuration file");
        fclose(file);
        return -1;
    }

    fclose(file);

    // The original 9 settings are required; OpenMP settings have defaults.
    for (int i = 0; i < 9; i++) {
        if (!seen[i]) {
            fprintf(stderr, "Missing configuration setting\n");
            return -1;
        }
    }

    // Validate allowed ranges
    if ( temp.batch_size <= 0 ||
        temp.max_temperature <= 0 ||
        temp.max_pressure <= 0 ||
        temp.washing_time <= 0 ||
        temp.crushing_time <= 0 ||
        temp.malaxation_time <= 0 ||
        temp.separation_time <= 0 ||
        temp.min_quality_score < 0 ||
        temp.min_quality_score > 100 ||
        temp.oil_extraction_rate < 0 ||
        temp.oil_extraction_rate > 1 ||
        temp.separation_water_share < 0 ||
        temp.separation_water_share > 1 ||
        temp.separation_portions <= 0 ||
        (temp.separation_use_openmp != 0 && temp.separation_use_openmp != 1) ||
temp.test_reject_batch < 0 ||
(temp.test_reject_reason != TEST_REJECT_NONE &&
 temp.test_reject_reason != TEST_REJECT_TEMPERATURE &&
 temp.test_reject_reason != TEST_REJECT_PRESSURE) ||
(temp.test_reject_batch == 0 &&
 temp.test_reject_reason != TEST_REJECT_NONE) ||
(temp.test_reject_batch > 0 &&
 temp.test_reject_reason == TEST_REJECT_NONE) ) {

        fprintf(stderr, "Error: Configuration values out of range\n");
        return -1;
    }

    // Apply configuration only after validation
    *config = temp;

    return 0;
}

void print_config(const FactoryConfig *config)
{
    if (config == NULL) {
        return;
    }

    printf("\n===== Factory Configuration =====\n");
    printf("Batch Size          : %.2f\n", config->batch_size);
    printf("Max Temperature     : %.2f\n", config->max_temperature);
    printf("Max Pressure        : %.2f\n", config->max_pressure);
    printf("Washing Time        : %d\n", config->washing_time);
    printf("Crushing Time       : %d\n", config->crushing_time);
    printf("Malaxation Time     : %d\n", config->malaxation_time);
    printf("Separation Time     : %d\n", config->separation_time);
    printf("Min Quality Score   : %d\n", config->min_quality_score);
    printf("Oil Extraction Rate : %.2f\n", config->oil_extraction_rate);
    printf("Separation Water Share: %.2f\n", config->separation_water_share);
    printf("Separation Portions : %d\n", config->separation_portions);
    printf("OpenMP Enabled      : %s\n", config->separation_use_openmp ? "yes" : "no");

const char *reason =
    config->test_reject_reason == TEST_REJECT_TEMPERATURE ? "TEMPERATURE" :
    config->test_reject_reason == TEST_REJECT_PRESSURE ? "PRESSURE" : "NONE";

printf("Test Reject Batch   : %d\n", config->test_reject_batch);
printf("Test Reject Reason  : %s\n", reason);


    printf("=================================\n");
}
