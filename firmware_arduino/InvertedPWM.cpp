#include "InvertedPWM.h"

static inline float __inverted_duty_cycle(float duty_cycle)
{
    return 100.0 - duty_cycle;
}

static inline float __clamp(float v, float min, float max)
{
    if (v > max) return max;
    else if (v < min) return min;
    return v;
}

static float __clamped_inverted_duty_cycle(float duty_cycle)
{
    return __inverted_duty_cycle(__clamp(duty_cycle, 0, 100));
}

InvertedPWM::InvertedPWM(pin_size_t pin, float frequency, float init_duty_cycle)
: _pwm(pin, frequency, __clamped_inverted_duty_cycle(init_duty_cycle)), _pin(pin)
{}

void InvertedPWM::set(float frequency, float duty_cycle)
{
    _pwm.setPWM(_pin, frequency, __clamped_inverted_duty_cycle(duty_cycle));
}