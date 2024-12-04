#include <sys/_stdint.h>
#ifndef _STEPPER_H_
#define _STEPPER_H_

#include <Arduino.h>

#define STEPPER_STEP_DELAY_US 1000L

class Stepper
{
public:
    enum StepDir : int8_t
    {
        FORWARD = 1,
        BACKWARD = -1
    };

protected:
    const uint8_t _pin_1, _pin_2, _pin_3, _pin_4;
    int8_t _curr_step = 0; // within the step cycle
    int32_t _curr_pos = 0; // absolute position of the stepper
    bool _begin_flag = false;

public:
    Stepper(uint8_t pin_1, uint8_t pin_2, uint8_t pin_3, uint8_t pin_4);
    bool begin();
    void release_stepper();

    bool move_steps_blocking(int32_t steps, bool release=true);
    bool move_to_pos_blocking(int32_t pos, bool release=true);

    inline int32_t get_curr_pos() const { return _curr_pos; }
    inline void set_curr_pos_forced(int32_t new_pos) { _curr_pos = new_pos; }

    bool is_save_state_ok() const;
    void reset_save_state();

public:
    // should never be called by the user!
    int32_t _make_step(StepDir dir);
};

/// async

class StepperAsync : public Stepper
{
public:
    StepperAsync(uint8_t pin_1, uint8_t pin_2, uint8_t pin_3, uint8_t pin_4)
    : Stepper(pin_1, pin_2, pin_3, pin_4) {}

    bool running() const;
    bool move_steps_async(int32_t steps, bool release=true);
    bool move_to_pos_async(int32_t pos, bool release=true);
    void move_steps_async_force(int32_t steps, bool release=true);
    void move_to_pos_async_force(int32_t pos, bool release=true);

    bool is_save_state_ok() const;
    void reset_save_state();
};

#endif /* _STEPPER_H_ */
