#include "servo_helper.h"
#include "debug_helper.h"


static_assert(SERVO_REAL_MIN_ANGLE <= SERVO_MIN_ANGLE, "macro SERVO_REAL_MIN_ANGLE cannot be greater than SERVO_MIN_ANGLE");
static_assert(SERVO_REAL_MAX_ANGLE >= SERVO_MAX_ANGLE, "macro SERVO_REAL_MIN_ANGLE cannot be greater than SERVO_MIN_ANGLE");

inline bool ServoRPI::_valid_angle(float angle)
{
    return angle >= _min_angle && angle <= _max_angle;
}

ServoRPI::ServoRPI(pin_size_t pin, bool invert_output, float min_angle, float max_angle, float init_angle)
: _pwm(pin, SERVO_FREQUENCY, 0), _pin(pin), _invert_output(invert_output), _min_angle(min_angle), _max_angle(max_angle), _curr_angle(init_angle)
{}

bool ServoRPI::set_angle(float angle)
{
    // calculate angle
    if (!_valid_angle(angle)) return false;
    const float normalized = (angle - SERVO_REAL_MIN_ANGLE) / SERVO_REAL_MAX_ANGLE;
    // TODO: test this!
    float duty_cycle = ((1 - normalized)*(_invert_output) + normalized*(!_invert_output) ) * 100;
    _pwm.set(SERVO_FREQUENCY, duty_cycle);
    _curr_angle = angle;
    return true;
}

void ServoRPI::detach()
{
    _pwm.set(SERVO_FREQUENCY, 100.0 * _invert_output);
}

static float __get_step(float angle, float curr_angle, float steps_per_angle)
{
    bool dest_greater = angle > curr_angle;
    int8_t dir = dest_greater ? 1 : -1;

    // float angles_to_cover = dest_greater ? angle - curr_angle : curr_angle - angle;
    // float n_steps = angles_to_cover * steps_per_angle;
    // float step_size = angles_to_cover / n_steps;
    // return step_size * dir;

    return abs(steps_per_angle) * dir;
}

static inline bool __is_set_angle_slow_finished(float final_angle, float curr_angle, float step)
{
    const bool forward = step > 0;
    const float penultimate_angle = final_angle - step;
    return (curr_angle >= penultimate_angle && forward) || (curr_angle <= penultimate_angle && !forward) || step == 0;
}

bool ServoRPI::set_angle_slow_blocking(float angle, float steps_per_angle, uint32_t delay_us_per_angle)
{
    _steps_per_angle = abs(steps_per_angle);
    _delay_us_per_angle = delay_us_per_angle;
    return set_angle_slow_blocking(angle);
}

bool ServoRPI::set_angle_slow_blocking(float angle)
{
    if (!_valid_angle(angle)) return false;
    if (angle == _curr_angle) return true;
    const float step = __get_step(angle, _curr_angle, _steps_per_angle);
    if (step == 0)
    {
        ERROR_PRINTLN("Cannot set angle with a step of 0!");
        return false;
    }

    // TODO: test this!
    bool forward = step > 0;
    const float angle_minus_step = angle - step;
    while (__is_set_angle_slow_finished(angle, _curr_angle, step))
    {
        if (!set_angle(_curr_angle + step))
        {
            ERROR_PRINTLN("This shouldn't be possible");
            return false;
        }
        sleep_us(_delay_us_per_angle);
    }
    if (!set_angle(angle))
    {
        ERROR_PRINTLN("This shouldn't be possible");
        return false;
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
    
    bool repeat = !__is_set_angle_slow_finished(_servo_timer_data.final_step, _servo_timer_data.curr_step, _servo_timer_data.step);
    if (!repeat)
    {
        _servo_timer_data.servo->set_angle(_servo_timer_data.final_step);
    }
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