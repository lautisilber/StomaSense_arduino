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
protected:
    enum StepType : uint8_t
    {
        NORMAL, WAVE, HALF
    };

    const uint8_t _pin_1, _pin_2, _pin_3, _pin_4;
    StepType _step_type;
    int8_t _curr_step = 0;
    int32_t _curr_pos = 0;

private:
    stepper_make_step_t _make_step;

public:
    Stepper(uint8_t pin_1, uint8_t pin_2, uint8_t pin_3, uint8_t pin_4, StepType step_type);
    void begin();
    void release_stepper();
    void set_step_type(StepType step_type);
    void make_step(StepperStepDir dir);
    void move_steps_blocking(int32_t steps, bool release=true);
    void move_to_pos_blocking(int32_t pos, bool release=true);
};

/// async

class StepperAsync : public Stepper
{
public:
    bool running() const;
    bool move_steps_async(int32_t steps, bool release=true);
    bool move_to_pos_async(int32_t pos, bool release=true);
    void move_steps_async_force(int32_t steps, bool release=true);
    void move_to_pos_async_force(int32_t pos, bool release=true);
};

#endif /* _STEPPER_H_ */