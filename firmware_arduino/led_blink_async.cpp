#include "led_blink_async.h"
#include "debug_helper.h"
#include "RPi_Pico_TimerInterrupt.h"


#include "timers.h" // use timer3

struct LedBlinkAsyncTimerData
{
    uint8_t pin;
    bool inverted;
};
static LedBlinkAsyncTimerData _timer_data;
static bool _led_blink_async_running = false;

static bool led_blink_timer_handler(struct repeating_timer *t)
{
    // false doesn't repeat, true repeats.
    digitalWrite(_timer_data.pin, LOW ^ _timer_data.inverted);
    _led_blink_async_running = false;
    return false;
}

bool led_blink_async(uint8_t pin, uint32_t delay_ms, bool inverted)
{
    if (_led_blink_async_running)
    {
        WARN_PRINTLN("Can't schedule async blink because it has already been scheduled");
        return false;
    }
    _timer_data.pin = pin;
    _timer_data.inverted = inverted;
    _led_blink_async_running = timer3.attachInterruptInterval(delay_ms * 1000, led_blink_timer_handler);
    if (_led_blink_async_running)
    {
        digitalWrite(pin, HIGH ^ inverted);
    }
    return _led_blink_async_running;
}