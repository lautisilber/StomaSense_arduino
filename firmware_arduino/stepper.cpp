#include "stepper.h"
#include "debug_helper.h"

#include "eeprom_helper.h"

static bool stepper_init_flag = false;

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

struct _StepperSaveState
{
    bool moving = false;
    int32_t pos;
};

[[noreturn]]
static inline void _reboot()
{
    sleep_ms(5000);
    rp2040.reboot();
    CRITICAL_PRINTLN("Should have rebooted! Idling now");
    for (;;) { sleep_ms(1000); } // should be unreachable
}

static inline void read_stepper_save_state(_StepperSaveState *s, uint32_t l)
{
    bool r = EEPROM_Helper::get(*s);
    if (!r)
    {
        CRITICAL_PRINTFLN("Couldn't read stepper save state (line %lu)", l);
        _reboot();
    }
}

static inline void write_stepper_save_state(bool moving, int32_t pos, uint32_t l)
{
    _StepperSaveState s = {.moving=moving, .pos=pos};
    bool r = EEPROM_Helper::put(s);
    if (!r)
    {
        CRITICAL_PRINTFLN("Couldn't write stepper save state (line %lu)", l);
        _reboot();
    }
    r = EEPROM_Helper::commit();
    if (!r)
    {
        CRITICAL_PRINTFLN("Couldn't commit stepper save state (line %lu)", l);
        _reboot();
    }
}

Stepper::Stepper(uint8_t pin_1, uint8_t pin_2, uint8_t pin_3, uint8_t pin_4, StepType step_type)
: _pin_1(pin_1), _pin_2(pin_2), _pin_3(pin_3), _pin_4(pin_4)
{
    if (stepper_init_flag)
    {
        CRITICAL_PRINTLN("Cannot have two Stepper instances");
        _reboot();
    }
    set_step_type(step_type);
}

bool Stepper::begin()
{
    pinMode(_pin_1, OUTPUT);
    pinMode(_pin_2, OUTPUT);
    pinMode(_pin_3, OUTPUT);
    pinMode(_pin_4, OUTPUT);

    EEPROM_Helper::begin<_StepperSaveState>();
    _StepperSaveState s;
    read_stepper_save_state(&s, __LINE__);
    if (s.moving)
    {
        CRITICAL_PRINTLN("System didn't complete a stepper movement. Stepper not initialized!");
    }
    else
    {
        _curr_pos = s.pos;
    }
    _begin_flag = s.moving;
    return s.moving;
}

void Stepper::release_stepper()
{
    if (!_begin_flag) return;
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
        __make_step = &_make_step_normal;
        break;
    case StepType::WAVE:
        __make_step = &_make_step_wave;
        break;
    case StepType::HALF:
        __make_step = &_make_step_half;
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

void Stepper::_make_step(StepperStepDir dir)
{
    if (!_begin_flag) return;
    __make_step(&_curr_step, &_curr_pos, dir, _pin_1, _pin_2, _pin_3, _pin_4);
}

void Stepper::move_steps_blocking(int32_t steps, bool release)
{
    if (steps == 0) return;
    const int32_t abs_steps = abs(steps);
    const StepperStepDir dir = (steps > 0 ? StepperStepDir::FORWARD : StepperStepDir::BACKWARD);

    write_stepper_save_state(true, _curr_pos, __LINE__);

    for (int32_t i = 0; i < abs_steps; i++)
    {
        _make_step(dir);
        sleep_us(STEPPER_STEP_DELAY_US);
    }

    write_stepper_save_state(false, _curr_pos, __LINE__);

    if (release)
        release_stepper();
}

void Stepper::move_to_pos_blocking(int32_t pos, bool release)
{
    const int32_t steps = pos - _curr_pos;
    move_steps_blocking(steps, release);
}

bool Stepper::is_save_state_ok() const
{
    _StepperSaveState s;
    read_stepper_save_state(&s, __LINE__);
    return !s.moving;
}

void Stepper::reset_save_state()
{
    _StepperSaveState s;
    read_stepper_save_state(&s, __LINE__);
    write_stepper_save_state(false, s.pos, __LINE__);
}



// async

#include "RPi_Pico_TimerInterrupt.h"

// RPI_PICO_Timer ITimer1(__COUNTER__);
static RPI_PICO_Timer stepperTimer(1);

struct StepperTimerData
{
    int32_t curr_pos, final_pos;
    Stepper *stepper;
    volatile bool running = false;
    bool release;
};
static StepperTimerData _stepper_timer_data;

static bool stepper_timer_handler(struct repeating_timer *t)
{
    // false doesn't repeat, true repeats.

    // TODO: fix this dumb line of code!!
    const StepperStepDir dir = (_stepper_timer_data.final_pos ? StepperStepDir::FORWARD : StepperStepDir::BACKWARD);
    _stepper_timer_data.stepper->_make_step(dir);
    _stepper_timer_data.curr_pos += dir;
    
    bool repeat = _stepper_timer_data.curr_pos != _stepper_timer_data.final_pos;

    if (!repeat)
    {
        write_stepper_save_state(false, _stepper_timer_data.curr_pos, __LINE__);

        if (_stepper_timer_data.release)
        {
            _stepper_timer_data.stepper->release_stepper();
        }
    }

    // this should be set last! (because of save state)
    _stepper_timer_data.running = repeat;
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

    _stepper_timer_data.curr_pos = 0;
    _stepper_timer_data.final_pos = steps;
    _stepper_timer_data.stepper = this;
    _stepper_timer_data.release = release;

    write_stepper_save_state(true, _curr_step, __LINE__);
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

bool StepperAsync::is_save_state_ok() const
{
    // if running, assume it's alright, otherwise, it shouldn't be moving
    if (running()) return true;
    return Stepper::is_save_state_ok();
}

void StepperAsync::reset_save_state()
{
    // if running, assume it's alright and change nothing, otherwise, it shouldn't be moving
    if (!running());
        Stepper::reset_save_state();
}
