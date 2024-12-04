#include "stepper.h"
#include "debug_helper.h"

#include "eeprom_helper.h"

static bool stepper_init_flag = false;

static const bool step_half[8][4] = {
  { 1, 0, 0, 0 },
  { 1, 1, 0, 0 },
  { 0, 1, 0, 0 },
  { 0, 1, 1, 0 },
  { 0, 0, 1, 0 },
  { 0, 0, 1, 1 },
  { 0, 0, 0, 1 },
  { 1, 0, 0, 1 }
};

#define MAX_STEP 7
#define EEPROM_BEGIN() EEPROM_Helper::begin<_StepperSaveState>()

struct _StepperSaveState {
    bool moving = false;
    int32_t pos;
};

[[noreturn]] static inline void _reboot() {
    sleep_ms(5000);
    rp2040.reboot();
    CRITICAL_PRINTLN("Should have rebooted! Idling now");
    for (;;) { sleep_ms(1000); }  // should be unreachable
}

static inline void read_stepper_save_state(_StepperSaveState *s, uint32_t l) {
    bool r = EEPROM_Helper::get(*s);
    if (!r) {
        CRITICAL_PRINTFLN("Couldn't read stepper save state (line %lu)", l);
        _reboot();
    }
}

static inline void write_stepper_save_state(bool moving, int32_t pos, uint32_t l) {
    _StepperSaveState s = { .moving = moving, .pos = pos };
    bool r = EEPROM_Helper::put(s);
    if (!r) {
        CRITICAL_PRINTFLN("Couldn't write stepper save state (line %lu)", l);
        _reboot();
    }
    r = EEPROM_Helper::commit();
    if (!r) {
        CRITICAL_PRINTFLN("Couldn't commit stepper save state (line %lu)", l);
        _reboot();
    }
}

Stepper::Stepper(uint8_t pin_1, uint8_t pin_2, uint8_t pin_3, uint8_t pin_4)
    : _pin_1(pin_1), _pin_2(pin_2), _pin_3(pin_3), _pin_4(pin_4) {
    if (stepper_init_flag) {
        CRITICAL_PRINTLN("Cannot have two Stepper instances");
        _reboot();
    }
}

bool Stepper::begin() {
    if (_begin_flag) return true;
    pinMode(_pin_1, OUTPUT);
    pinMode(_pin_2, OUTPUT);
    pinMode(_pin_3, OUTPUT);
    pinMode(_pin_4, OUTPUT);

    EEPROM_BEGIN();
    _StepperSaveState s;
    read_stepper_save_state(&s, __LINE__);
    if (s.moving) {
        CRITICAL_PRINTLN("System didn't complete a stepper movement. Stepper not initialized!");
    } else {
        _curr_pos = s.pos;
    }
    _begin_flag = !s.moving;
    return _begin_flag;
}

void Stepper::release_stepper() {
    if (!_begin_flag)
    {
        WARN_PRINTLN("Trying to release stepper when it wasn't initialized");
        return;
    }
    digitalWrite(_pin_1, LOW);
    digitalWrite(_pin_2, LOW);
    digitalWrite(_pin_3, LOW);
    digitalWrite(_pin_4, LOW);
}

int32_t Stepper::_make_step(StepDir dir) {
    // returns _curr_pos
    _curr_step += dir;
    if (_curr_step < 0) _curr_step = MAX_STEP;
    else if (_curr_step > MAX_STEP) _curr_step = 0;

    digitalWrite(_pin_1, step_half[_curr_step][0]);
    digitalWrite(_pin_2, step_half[_curr_step][1]);
    digitalWrite(_pin_3, step_half[_curr_step][2]);
    digitalWrite(_pin_4, step_half[_curr_step][3]);

    _curr_pos += dir;
    return _curr_pos;
}

bool Stepper::move_steps_blocking(int32_t steps, bool release) {
    if (!_begin_flag)
    {
        WARN_PRINTLN("Trying to move stepper when it wasn't initialized");
        return false;
    }
    if (steps == 0) return true;
    const int32_t abs_steps = abs(steps);
    const StepDir dir = (steps > 0 ? StepDir::FORWARD : StepDir::BACKWARD);

    write_stepper_save_state(true, _curr_pos, __LINE__);

    for (int32_t i = 0; i < abs_steps; i++) {
        _make_step(dir);
        sleep_us(STEPPER_STEP_DELAY_US);
    }

    write_stepper_save_state(false, _curr_pos, __LINE__);

    if (release)
        release_stepper();
    return true;
}

bool Stepper::move_to_pos_blocking(int32_t pos, bool release) {
    const int32_t steps = pos - _curr_pos;
    return move_steps_blocking(steps, release);
}

bool Stepper::is_save_state_ok() const {
    EEPROM_BEGIN();
    _StepperSaveState s;
    read_stepper_save_state(&s, __LINE__);
    return !s.moving;
}

void Stepper::reset_save_state() {
    EEPROM_BEGIN();
    _StepperSaveState s;
    read_stepper_save_state(&s, __LINE__);
    write_stepper_save_state(false, s.pos, __LINE__);
}



// async

#include "RPi_Pico_TimerInterrupt.h"

// RPI_PICO_Timer ITimer1(__COUNTER__);
static RPI_PICO_Timer stepperTimer(1);

struct StepperTimerData {
    int32_t curr_step, final_step;
    Stepper::StepDir step_dir;
    Stepper *stepper;
    volatile bool running = false;
    bool release;
};
static StepperTimerData _stepper_timer_data;

static bool stepper_timer_handler(struct repeating_timer *t) {
    // false doesn't repeat, true repeats.

    int32_t curr_pos = _stepper_timer_data.stepper->_make_step(_stepper_timer_data.step_dir);
    _stepper_timer_data.curr_step += _stepper_timer_data.step_dir;

    bool repeat = _stepper_timer_data.curr_step != _stepper_timer_data.final_step;

    if (!repeat) {
        write_stepper_save_state(false, curr_pos, __LINE__);

        if (_stepper_timer_data.release) {
            _stepper_timer_data.stepper->release_stepper();
        }
    }

    // this should be set last! (because of save state)
    _stepper_timer_data.running = repeat;
    return repeat;
}

bool StepperAsync::running() const {
    return _stepper_timer_data.running;
}

bool StepperAsync::move_steps_async(int32_t steps, bool release) {
    if (!_begin_flag)
    {
        WARN_PRINTLN("Trying to move stepper when it wasn't initialized");
        return false;
    }
    if (steps == 0) return true;
    if (running()) {
        WARN_PRINTLN("StepperAsync: Can't start new move operation because one is already running");
        return false;
    }

    _stepper_timer_data.curr_step = 0;
    _stepper_timer_data.final_step = steps;
    _stepper_timer_data.step_dir = (steps > 0 ? StepDir::FORWARD : StepDir::BACKWARD);
    _stepper_timer_data.stepper = this;
    _stepper_timer_data.release = release;

    write_stepper_save_state(true, _curr_step, __LINE__);
    _stepper_timer_data.running = stepperTimer.attachInterruptInterval(STEPPER_STEP_DELAY_US, stepper_timer_handler);

    return running();
}

bool StepperAsync::move_to_pos_async(int32_t pos, bool release) {
    const int32_t steps = pos - _curr_pos;
    return move_steps_async(steps, release);
}

void StepperAsync::move_steps_async_force(int32_t steps, bool release) {
  while (!move_steps_async(steps, release))
    sleep_ms(2 * STEPPER_STEP_DELAY_US);
}

void StepperAsync::move_to_pos_async_force(int32_t pos, bool release) {
  const int32_t steps = pos - _curr_pos;
  move_steps_async_force(steps, release);
}

bool StepperAsync::is_save_state_ok() const {
  // if running, assume it's alright, otherwise, it shouldn't be moving
  if (running()) return true;
  return Stepper::is_save_state_ok();
}

void StepperAsync::reset_save_state() {
  // if running, assume it's alright and change nothing, otherwise, it shouldn't be moving
  if (!running())
    return;
  Stepper::reset_save_state();
}
