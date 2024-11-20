#include "servo_helper.h"
#include "debug_helper.h"

inline bool ServoRPI::_valid_angle(float angle)
{
    return angle >= _min_angle && angle <= _max_angle;
}

ServoRPI::ServoRPI(pin_size_t pin, uint16_t servo_min_duty_us, uint16_t servo_max_duty_us, float min_angle, float max_angle, float init_angle)
: _pwm(pin, SERVO_FREQUENCY, 0), _pin(pin), _min_angle(min_angle), _max_angle(max_angle), _curr_angle(init_angle)
{}

bool ServoRPI::set_angle(float angle)
{
    // calculate angle
    if (!_valid_angle(angle)) return false;
    float duty_cycle = angle * 100;
    _pwm.set(SERVO_FREQUENCY, duty_cycle);
    _curr_angle = angle;
    return true;
}

static float __get_step(float angle, float curr_angle, uint32_t n_steps)
{
    bool dest_greater = angle > curr_angle;
    int8_t dir = dest_greater ? 1 : -1;
    float angle_diff = dest_greater ? angle - curr_angle : curr_angle - angle;
    float step_size = angle_diff / n_steps;
    float step = step_size * dir;
    return step;
}

bool ServoRPI::set_angle_slow_blocking(float angle, uint32_t n_steps, uint32_t delay_us)
{
    if (!_valid_angle(angle)) return false;
    if (angle == _curr_angle) return true;
    float step = __get_step(angle, _curr_angle, n_steps);
    for (uint32_t i = 0; i < n_steps; i++)
    {
        set_angle(_curr_angle + step);
        sleep_us(delay_us);
    }
    return true;
}



#include "RPi_Pico_TimerInterrupt.h"

// RPI_PICO_Timer ITimer1(__COUNTER__);
static RPI_PICO_Timer servoTimer(2);

struct ServoTimerData
{
    float curr_step, final_step, step;
    ServoRPI *servo;
    volatile bool running = false;
};
static ServoTimerData _servo_timer_data;

static bool servo_timer_handler(struct repeating_timer *t)
{
    // false doesn't repeat, true repeats.
    
    _servo_timer_data.curr_step += _servo_timer_data.step;
    _servo_timer_data.servo->set_angle(_servo_timer_data.curr_step);
    
    bool repeat = _servo_timer_data.step > 0 ? _servo_timer_data.curr_step >= _servo_timer_data.final_step : _servo_timer_data.curr_step <= _servo_timer_data.final_step;
    _servo_timer_data.running = repeat;
    return repeat;
}

bool ServoRPIAsync::running() const
{
    return _servo_timer_data.running;
}

bool ServoRPIAsync::set_angle_slow_async(float angle, uint32_t n_steps, uint32_t delay_us)
{
    if (n_steps == 0 || angle == _curr_angle) return true;
    if (delay_us == 0)
    {
        _servo_timer_data.running = true;
        bool res = set_angle_slow_blocking(angle, n_steps, 0);
        _servo_timer_data.running = false;
        return res;
    }

    if (running())
    {
        WARN_PRINTLN("ServoRPIAsync: Can't start new set_angle operation because one is already running");
        return false;
    }

    _servo_timer_data.step = __get_step(angle, _curr_angle, n_steps);
    _servo_timer_data.curr_step = _curr_angle;
    _servo_timer_data.final_step = angle;
    _servo_timer_data.servo = this;
    _servo_timer_data.running = servoTimer.attachInterruptInterval(delay_us, servo_timer_handler);
    
    return running();
}