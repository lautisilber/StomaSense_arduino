#ifndef _PUMP_HELPER_H_
#define _PUMP_HELPER_H_

#include <Arduino.h>
#include "InvertedPWM.h"

#define PUMP_DEFAULT_FREQUENCY_HZ 2000

class Pump
{
private:
    float _frequency = PUMP_DEFAULT_FREQUENCY_HZ;
    InvertedPWM _pwm;

protected:
    inline void pump_start(float intensity);
public:
    inline void pump_stop();

public:
    Pump(pin_size_t pin, float frequency=PUMP_DEFAULT_FREQUENCY_HZ);

    void set_frequency(float frequency);
    void pump_blocking(float intensity, uint32_t time_us);
};



class PumpAsync : public Pump
{
public:
    bool running() const;
    bool pump_async(float intensity, uint32_t time_us);
};


#endif /* _PUMP_HELPER_H_ */