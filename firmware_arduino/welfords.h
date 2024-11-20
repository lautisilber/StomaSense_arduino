#ifndef _WELFORDS_H_
#define _WELFORDS_H_

#include <Arduino.h>
// https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance

namespace Welfords
{
    struct Aggregate
    {
        uint32_t count = 0;
        float mean = 0, M2 = 0;
    };

    void update(Aggregate *agg, float new_value);
    bool finalize(Aggregate *agg, float *mean, float *stdev);
}

#endif /* _WELFORDS_H_ */