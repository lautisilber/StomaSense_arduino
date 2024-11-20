#ifndef _INVERTED_PWM_H_
#define _INVERTED_PWM_H_

#include <Arduino.h>
#include <RP2040_PWM.h>

class InvertedPWM
{
private:
    RP2040_PWM _pwm;
    const pin_size_t _pin;
public:
    InvertedPWM(pin_size_t pin, float frequency, float init_duty_cycle);
    void set(float frequency, float duty_cycle);
};

#endif /* _INVERTED_PWM_H_ */