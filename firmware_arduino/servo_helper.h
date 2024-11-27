#pragma once

#include <Arduino.h>
#include "InvertedPWM.h"
#include "defs.h"

#define SERVO_FREQUENCY 50

class ServoRPI
{
private:
    const pin_size_t _pin;
    const bool _invert_output;
    InvertedPWM _pwm;
    const float _min_angle, _max_angle;

protected:
    float _curr_angle;
    inline bool _valid_angle(float angle);

    float _steps_per_angle = SERVO_SLOW_DEFAULT_STEPS_PER_ANGLE;
    uint32_t _delay_us_per_angle = SERVO_SLOW_DEFAULT_DELAY_US_PER_ANGLE;

public:
    ServoRPI(pin_size_t pin, bool invert_output, float min_angle=SERVO_MIN_ANGLE, float max_angle=SERVO_MAX_ANGLE, float init_angle=(SERVO_MIN_ANGLE + SERVO_MAX_ANGLE) / 2);

    bool set_angle(float angle);
    bool set_angle_slow_blocking(float angle);
    bool set_angle_slow_blocking(float angle, float steps_per_angle, uint32_t delay_us_per_angle);

    inline uint8_t get_curr_angle() const { return _curr_angle; }
    void detach();
};


class ServoRPIAsync : public ServoRPI
{
public:
    bool running() const;
    bool set_angle_slow_async(float angle, uint32_t n_steps, uint32_t delay_us);
};