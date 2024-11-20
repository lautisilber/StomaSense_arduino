#include "pump_helper.h"
#include "debug_helper.h"

Pump::Pump(pin_size_t pin, float frequency)
: _pwm(pin, frequency, 0), _frequency(frequency)
{}

void Pump::set_frequency(float frequency)
{
    _frequency = frequency;
}

inline void Pump::pump_start(float intensity)
{
    _pwm.set(_frequency, intensity);
}

inline void Pump::pump_stop()
{
    _pwm.set(_frequency, 0);
}

void Pump::pump_blocking(float intensity, uint32_t time_us)
{
    pump_start(intensity);
    sleep_us(time_us);
    pump_stop();
}





#include "RPi_Pico_TimerInterrupt.h"

// RPI_PICO_Timer ITimer1(__COUNTER__);
static RPI_PICO_Timer pumpTimer(3);

struct PumpTimerData
{
    Pump *pump;
    bool running = false;
};
static PumpTimerData _pump_timer_data;

static bool pump_timer_handler(struct repeating_timer *t)
{
    // false doesn't repeat, true repeats.
    _pump_timer_data.pump->pump_stop();
    _pump_timer_data.running = false;
    return false;
}

bool PumpAsync::running() const
{
    return _pump_timer_data.running;
}

bool PumpAsync::pump_async(float intensity, uint32_t time_us)
{
    if (!time_us) return false;
    if (running())
    {
        WARN_PRINTLN("PumpAsync: Can't start new pump operation because one is already running");
        return false;
    }
    _pump_timer_data.pump = this;
    _pump_timer_data.running = pumpTimer.attachInterruptInterval(time_us, pump_timer_handler);
    return _pump_timer_data.running;
}
