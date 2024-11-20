#include "stepper.h"
#include "debug_helper.h"

static const bool step_normal[4][4] = {
    {1, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 0, 0, 1}
};

static const bool step_wave[4][4] = {
    {1, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 1},
    {1, 0, 0, 1}
};

static const bool step_half[8][4] = {
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {0, 1, 0, 0},
    {0, 1, 1, 0},
    {0, 0, 1, 0},
    {0, 0, 1, 1},
    {0, 0, 0, 1},
    {1, 0, 0, 1}
};

static void _make_step_normal(int8_t *curr_step, int32_t *curr_pos, StepperStepDir dir, uint8_t pin_1, uint8_t pin_2, uint8_t pin_3, uint8_t pin_4)
{
    const uint8_t last_step = 3;
    (*curr_step) += dir;
    if ((*curr_step) < 0) (*curr_step) = last_step;
    else if ((*curr_step) > last_step) (*curr_step) = 0;

    digitalWrite(pin_1, step_normal[(*curr_step)][0]);
    digitalWrite(pin_2, step_normal[(*curr_step)][1]);
    digitalWrite(pin_3, step_normal[(*curr_step)][2]);
    digitalWrite(pin_4, step_normal[(*curr_step)][3]);

    (*curr_pos) += dir*2;
}

static void _make_step_wave(int8_t *curr_step, int32_t *curr_pos, StepperStepDir dir, uint8_t pin_1, uint8_t pin_2, uint8_t pin_3, uint8_t pin_4)
{
    const uint8_t last_step = 3;
    (*curr_step) += dir;
    if ((*curr_step) < 0) (*curr_step) = last_step;
    else if ((*curr_step) > last_step) (*curr_step) = 0;

    digitalWrite(pin_1, step_wave[(*curr_step)][0]);
    digitalWrite(pin_2, step_wave[(*curr_step)][1]);
    digitalWrite(pin_3, step_wave[(*curr_step)][2]);
    digitalWrite(pin_4, step_wave[(*curr_step)][3]);

    (*curr_pos) += dir*2;
}

static void _make_step_half(int8_t *curr_step, int32_t *curr_pos, StepperStepDir dir, uint8_t pin_1, uint8_t pin_2, uint8_t pin_3, uint8_t pin_4)
{
    const uint8_t last_step = 7;
    (*curr_step) += dir;
    if ((*curr_step) < 0) (*curr_step) = last_step;
    else if ((*curr_step) > last_step) (*curr_step) = 0;

    digitalWrite(pin_1, step_half[(*curr_step)][0]);
    digitalWrite(pin_2, step_half[(*curr_step)][1]);
    digitalWrite(pin_3, step_half[(*curr_step)][2]);
    digitalWrite(pin_4, step_half[(*curr_step)][3]);

    (*curr_pos) += dir;
}

Stepper::Stepper(uint8_t pin_1, uint8_t pin_2, uint8_t pin_3, uint8_t pin_4, StepType step_type)
: _pin_1(pin_1), _pin_2(pin_2), _pin_3(pin_3), _pin_4(pin_4)
{
    set_step_type(step_type);
}

void Stepper::begin()
{
    pinMode(_pin_1, OUTPUT);
    pinMode(_pin_2, OUTPUT);
    pinMode(_pin_3, OUTPUT);
    pinMode(_pin_4, OUTPUT);
}

void Stepper::release_stepper()
{
    digitalWrite(_pin_1, LOW);
    digitalWrite(_pin_2, LOW);
    digitalWrite(_pin_3, LOW);
    digitalWrite(_pin_4, LOW);
}

void Stepper::set_step_type(StepType step_type)
{
    switch(step_type)
    {
    case StepType::NORMAL:
        _make_step = &_make_step_normal;
        break;
    case StepType::WAVE:
        _make_step = &_make_step_wave;
        break;
    case StepType::HALF:
        _make_step = &_make_step_half;
        break;
    default:
        Serial.printf("Error -> Stepper: Cannot set step_type to %i\n", _step_type);
        return;
    }

    if (step_type != StepType::HALF && _step_type == StepType::HALF)
    {
        _curr_step /= 2;
    }
    else if (step_type == StepType::HALF && _step_type != StepType::HALF)
    {
        _curr_step *= 2;
    }
    _step_type = step_type;
}

void Stepper::make_step(StepperStepDir dir)
{
    _make_step(&_curr_step, &_curr_pos, dir, _pin_1, _pin_2, _pin_3, _pin_4);
}

void Stepper::move_steps_blocking(int32_t steps, bool release)
{
    if (steps == 0) return;
    const int32_t abs_steps = abs(steps);
    const StepperStepDir dir = (steps > 0 ? StepperStepDir::FORWARD : StepperStepDir::BACKWARD);
    for (int32_t i = 0; i < abs_steps; i++)
    {
        make_step(dir);
        sleep_us(STEPPER_STEP_DELAY_US);
    }
    if (release)
        release_stepper();
}

void Stepper::move_to_pos_blocking(int32_t pos, bool release)
{
    const int32_t steps = pos - _curr_pos;
    move_steps_blocking(steps, release);
}




// async

#include "RPi_Pico_TimerInterrupt.h"

// RPI_PICO_Timer ITimer1(__COUNTER__);
static RPI_PICO_Timer stepperTimer(1);

struct StepperTimerData
{
    int32_t curr_step, final_step;
    Stepper *stepper;
    volatile bool running = false;
    bool release;
};
static StepperTimerData _stepper_timer_data;

static bool stepper_timer_handler(struct repeating_timer *t)
{
    // false doesn't repeat, true repeats.

    // TODO: fix this dumb line of code!!
    const StepperStepDir dir = (_stepper_timer_data.final_step ? StepperStepDir::FORWARD : StepperStepDir::BACKWARD);
    _stepper_timer_data.stepper->make_step(dir);
    _stepper_timer_data.curr_step += dir;
    
    bool repeat = _stepper_timer_data.curr_step != _stepper_timer_data.final_step;
    _stepper_timer_data.running = repeat;
    if (!repeat && _stepper_timer_data.release)
        _stepper_timer_data.stepper->release_stepper();
    return repeat;
}

bool StepperAsync::running() const
{
    return _stepper_timer_data.running;
}

bool StepperAsync::move_steps_async(int32_t steps, bool release)
{
    if (steps == 0) return true;
    if (running())
    {
        WARN_PRINTLN("StepperAsync: Can't start new move operation because one is already running");
        return false;
    }

    _stepper_timer_data.curr_step = 0;
    _stepper_timer_data.final_step = steps;
    _stepper_timer_data.stepper = this;
    _stepper_timer_data.release = release;
    _stepper_timer_data.running = stepperTimer.attachInterruptInterval(STEPPER_STEP_DELAY_US, stepper_timer_handler);
    
    return running();
}

bool StepperAsync::move_to_pos_async(int32_t pos, bool release)
{
    const int32_t steps = pos - _curr_pos;
    return move_steps_async(steps, release);
}

void StepperAsync::move_steps_async_force(int32_t steps, bool release)
{
    while (!move_steps_async(steps, release))
        sleep_ms(2 * STEPPER_STEP_DELAY_US);
}

void StepperAsync::move_to_pos_async_force(int32_t pos, bool release)
{
    const int32_t steps = pos - _curr_pos;
    move_steps_async_force(steps, release);
}
