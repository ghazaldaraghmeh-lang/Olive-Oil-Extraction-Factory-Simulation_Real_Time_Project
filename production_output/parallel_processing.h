#ifndef PARTNER3_PARALLEL_PROCESSING_H
#define PARTNER3_PARALLEL_PROCESSING_H

int calculate_separation_yields(double olive_weight,
                                double extraction_rate,
                                double water_share,
                                int virtual_portions,
                                int use_openmp,
                                double *oil, double *water, double *pomace);
#endif
