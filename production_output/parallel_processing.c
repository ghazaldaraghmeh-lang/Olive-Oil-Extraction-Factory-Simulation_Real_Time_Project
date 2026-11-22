/* OpenMP experiments and parallel processing implementation. */
#include "parallel_processing.h"
#include <math.h>
#include <omp.h>

int calculate_separation_yields(double weight, double rate,
                                double water_share, int portions, int use_openmp,
                                double *oil, double *water, double *pomace)
{
    if (!oil || !water || !pomace || portions < 1 || !isfinite(weight) ||
        weight <= 0.0 || !isfinite(rate) || rate < 0.0 || rate > 1.0 ||
        !isfinite(water_share) || water_share < 0.0 || water_share > 1.0)
        return -1;

    double oil_total = 0.0;
    double water_total = 0.0;
    double pomace_total = 0.0;
    const double portion_weight = weight / (double)portions;

#pragma omp parallel for if(use_openmp) schedule(static) reduction(+:oil_total,water_total,pomace_total)
    for (int i = 0; i < portions; i++) {
        double local_oil = portion_weight * rate;
        double remainder = portion_weight - local_oil;
        double local_water = remainder * water_share;
        double local_pomace = remainder - local_water;
        oil_total += local_oil;
        water_total += local_water;
        pomace_total += local_pomace;
    }
    if (!isfinite(oil_total) || !isfinite(water_total) ||
        !isfinite(pomace_total))
        return -1;
    *oil = oil_total;
    *water = water_total;
    *pomace = pomace_total;
    return 0;
}
