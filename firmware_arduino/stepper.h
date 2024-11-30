#ifndef _STEPPER_H_
#define _STEPPER_H_

#include <Arduino.h>

#define STEPPER_STEP_DELAY_US 1000L

enum StepperStepDir : int8_t
{
    FORWARD = 1,
    BACKWARD = -1
};

// takes in the step direction, makes the step, updates the current step
typedef void (*stepper_make_step_t)(int8_t*, int32_t*, StepperStepDir, uint8_t, uint8_t, uint8_t, uint8_t);

class Stepper
{
public:
    enum StepType : uint8_t
    {
        NORMAL, WAVE, HALF
    };

protected:
    const uint8_t _pin_1, _pin_2, _pin_3, _pin_4;
    StepType _step_type;
    int8_t _curr_step = 0;
    int32_t _curr_pos = 0;

private:
    stepper_make_step_t __make_step;
    bool _begin_flag = false;

public:
    Stepper(uint8_t pin_1, uint8_t pin_2, uint8_t pin_3, uint8_t pin_4, StepType step_type);
    bool begin();
    void release_stepper();
    void set_step_type(StepType step_type);

    void move_steps_blocking(int32_t steps, bool release=true);
    void move_to_pos_blocking(int32_t pos, bool release=true);

    inline int32_t get_curr_pos() const { return _curr_pos; }
    inline void set_curr_pos_forced(int32_t new_pos) { _curr_pos = new_pos; }

    bool is_save_state_ok() const;
    void reset_save_state();

public:
    // should never be called by the user!
    void _make_step(StepperStepDir dir);
};

/// async

class StepperAsync : public Stepper
{
public:
    StepperAsync(uint8_t pin_1, uint8_t pin_2, uint8_t pin_3, uint8_t pin_4, StepType step_type)
    : Stepper(pin_1, pin_2, pin_3, pin_4, step_type) {}

    bool running() const;
    bool move_steps_async(int32_t steps, bool release=true);
    bool move_to_pos_async(int32_t pos, bool release=true);
    void move_steps_async_force(int32_t steps, bool release=true);
    void move_to_pos_async_force(int32_t pos, bool release=true);

    bool is_save_state_ok() const;
    void reset_save_state();
};

#endif /* _STEPPER_H_ */
