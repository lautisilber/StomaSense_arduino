#include "welfords.h"

void Welfords::update(Aggregate *agg, float new_value)
{
    float delta, delta2;
    agg->count += 1;
    delta = new_value - agg->mean;
    agg->mean += delta / agg->count;
    delta2 = new_value - agg->mean;
    agg->M2 += delta * delta2;
}

bool Welfords::finalize(Aggregate *agg, float *mean, float *stdev)
{
    if (agg->count < 2) return false;
    (*mean) = agg->mean;
    // float variance = agg->M2 / agg->count;
    float sample_variance = agg->M2 / (agg->count - 1);
    (*stdev) = sqrt(sample_variance);
    return true;
}