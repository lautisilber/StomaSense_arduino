#pragma once

#include <Arduino.h>
#include "InvertedPWM.h"

#define SERVO_FREQUENCY 50

class ServoRPI
{
private:
    const pin_size_t _pin;
    InvertedPWM _pwm;
    const float _min_angle, _max_angle;

protected:
    float _curr_angle;
    inline bool _valid_angle(float angle);

public:
    ServoRPI(pin_size_t pin, uint16_t servo_min_duty_us, uint16_t servo_max_duty_us, float min_angle=1, float max_angle=179, float init_angle=90);

    bool set_angle(float angle);
    bool set_angle_slow_blocking(float angle, uint32_t n_steps, uint32_t delay_us);
};





class ServoRPIAsync : public ServoRPI
{
public:
    bool running() const;
    bool set_angle_slow_async(float angle, uint32_t n_steps, uint32_t delay_us);
};