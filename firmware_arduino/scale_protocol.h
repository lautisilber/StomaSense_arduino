#include "ArduinoJson/Object/JsonObject.hpp"
#include "ArduinoJson/Array/JsonArray.hpp"
#include <sys/_stdint.h>
#ifndef _SCALE_PROTOCOL_H_
#define _SCALE_PROTOCOL_H_

#include <Arduino.h>
#include <ArduinoJson.h>

#include "PollingTimer.h"
#include "debug_helper.h"

struct ScaleProtocolStepWait
{
    // data
    unsigned long wait_ms;

    // vars
    unsigned long begin_time_ms;
};

struct ScaleProtocolStepHoldWeight
{
    // data
    float weight;
    float acceptance_width;
    unsigned long wait_ms;

    // vars
    unsigned long begin_time_ms;
    // float begin_weight;

    // weight_should_be_greater is a poorly named varialbe. if true, it means that the current weight should be greater to the goal weight
    // in order to accomplish the weight goal. If false, the current weight should be lower than the gola weight in order
    // for the goal weight to be met
    bool weight_should_be_greater;
};

struct ScaleProtocolStepOscilateInRange
{
    // data
    float lower_weight, upper_weight;
    uint8_t n_cycles;
    unsigned long wait_ms;

    // vars
    uint8_t curr_cycle;
    unsigned int begin_time_ms;
};

#define SCALE_PROTOCOL_STEP_JSON_TYPE_KEY "t"

#define SCALE_PROTOCOL_STEP_JSON_WAIT_WAIT_MS_KEY "w"

#define SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_WEIGHT_KEY "w"
#define SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_ACCEPTANCE_WIDTH_KEY "a"
#define SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_WAIT_MS_KEY "m"

#define SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_LOWER_WEIGHT_KEY "l"
#define SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_UPPER_WEIGHT_KEY "u"
#define SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_N_CYCLES_KEY "n"
#define SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_WAIT_MS_KEY "t"

union ScaleProtocolStepUnion
{
    ScaleProtocolStepWait wait;
    ScaleProtocolStepHoldWeight hold_weight;
    ScaleProtocolStepOscilateInRange oscilate_in_range;
};

class ScaleProtocolStep
{
    /*
     * A scale protocol step is a customizable step in a list of steps that make up the protocol for a given scale.
     * The protocol of a scale is the particular conditions one wants to put the plant in throughout time.
     *
     * A step has access to the follwing variables:
     * - weight
     * - millis
     * - last step
     */
    
public:
    enum Type : uint8_t
    {
        WAIT = 1, HOLD_WEIGHT = 2, OSCILATE_IN_RANGE = 3
    };

private:
    uint8_t _internal_step; // set to 0 on the tick of completion of the step. 0 represents the first step
    Type _type;
    ScaleProtocolStepUnion _step_data;

public:
    void to_json(JsonObject *obj)
    {
        (*obj)[SCALE_PROTOCOL_STEP_JSON_TYPE_KEY] = _type;
        switch (_type) {
        case WAIT:
            (*obj)[SCALE_PROTOCOL_STEP_JSON_WAIT_WAIT_MS_KEY] = _step_data.wait.wait_ms;
            break;
        case HOLD_WEIGHT:
            (*obj)[SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_WEIGHT_KEY] = _step_data.hold_weight.weight;
            (*obj)[SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_ACCEPTANCE_WIDTH_KEY] = _step_data.hold_weight.acceptance_width;
            (*obj)[SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_WAIT_MS_KEY] = _step_data.hold_weight.wait_ms;
            break;
        case OSCILATE_IN_RANGE:
            (*obj)[SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_LOWER_WEIGHT_KEY] = _step_data.oscilate_in_range.lower_weight;
            (*obj)[SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_UPPER_WEIGHT_KEY] = _step_data.oscilate_in_range.upper_weight;
            (*obj)[SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_N_CYCLES_KEY] = _step_data.oscilate_in_range.n_cycles;
            (*obj)[SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_WAIT_MS_KEY] = _step_data.oscilate_in_range.wait_ms;
            break;
        default:
            ERROR_PRINTLN("Should be unreachable");
            break;
        }
    }

    bool from_json(JsonObject *obj)
    {
        if (!(*obj)[SCALE_PROTOCOL_STEP_JSON_TYPE_KEY].is<uint8_t>())
        {
            ERROR_PRINTFLN("Scale protocol step json doesn't contain key '%s' of type uint8_t");
            return false;
        }
        uint8_t type = (*obj)[SCALE_PROTOCOL_STEP_JSON_TYPE_KEY].as<uint8_t>();
        if (type < WAIT || type > OSCILATE_IN_RANGE)
        {
            ERROR_PRINTFLN("Scale protocol step json type is not a valid enum Type (it is %u)", type);
            return false;
        }
        _type = (Type)type;

        switch (_type)
        {
        case WAIT:
            if (!(*obj)[SCALE_PROTOCOL_STEP_JSON_WAIT_WAIT_MS_KEY].is<uint32_t>())
            {
                ERROR_PRINTFLN("Scale protocol step json wait error casting [%s -> uint32_t]", SCALE_PROTOCOL_STEP_JSON_WAIT_WAIT_MS_KEY);
                return false;
            }
            _step_data.wait.wait_ms = (*obj)[SCALE_PROTOCOL_STEP_JSON_WAIT_WAIT_MS_KEY].as<uint32_t>();
            break;
        case HOLD_WEIGHT:
            if (!(*obj)[SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_WEIGHT_KEY].is<float>() ||
                !(*obj)[SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_ACCEPTANCE_WIDTH_KEY].is<float>() ||
                !(*obj)[SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_WAIT_MS_KEY].is<uint32_t>())
            {
                ERROR_PRINTFLN("Scale protocol step json hold_weight error casting [%s -> float; %s -> float; %s -> uint32_t]",
                    SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_WEIGHT_KEY, SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_ACCEPTANCE_WIDTH_KEY, SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_WAIT_MS_KEY);
                return false;
            }
            _step_data.hold_weight.weight = (*obj)[SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_WEIGHT_KEY].is<float>();
            _step_data.hold_weight.acceptance_width = (*obj)[SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_ACCEPTANCE_WIDTH_KEY].is<float>();
            _step_data.hold_weight.wait_ms = (*obj)[SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_WAIT_MS_KEY].is<uint32_t>();
            break;
        case OSCILATE_IN_RANGE:
            if (!(*obj)[SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_LOWER_WEIGHT_KEY].as<float>() ||
                !(*obj)[SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_UPPER_WEIGHT_KEY].as<float>() ||
                !(*obj)[SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_N_CYCLES_KEY].as<uint8_t>() ||
                !(*obj)[SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_WAIT_MS_KEY].as<uint32_t>())
            {
                ERROR_PRINTFLN("Scale protocol step json wait oscilate_in_range casting [%s -> float; %s -> float; %s -> uint32_t]",
                    SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_WEIGHT_KEY, SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_ACCEPTANCE_WIDTH_KEY, SCALE_PROTOCOL_STEP_JSON_HOLD_WEIGHT_WAIT_MS_KEY);
                return false;
            }
            _step_data.oscilate_in_range.lower_weight = (*obj)[SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_LOWER_WEIGHT_KEY].as<float>();
            _step_data.oscilate_in_range.upper_weight = (*obj)[SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_UPPER_WEIGHT_KEY].as<float>();
            _step_data.oscilate_in_range.n_cycles = (*obj)[SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_N_CYCLES_KEY].as<uint8_t>();
            _step_data.oscilate_in_range.wait_ms = (*obj)[SCALE_PROTOCOL_STEP_JSON_OSCILATE_IN_RANGE_WAIT_MS_KEY].as<uint32_t>();
            break;
        default:
            ERROR_PRINTLN("Should be unreachable");
            break;
        }
        return true;
    }

    void run(float weight, ScaleProtocolStep *previous_step, bool *finished, bool *should_water)
    {
        *finished = false;
        *should_water = false;
        switch (_type)
        {
        case WAIT:
            if (_step_data.wait.wait_ms == 0)
            {
                // at the beginning of the wait step, if wait is 0, complete step
                goto next_step;
            }
            else
            {
                if (_internal_step == 0)
                {
                    // at the beginning of the wait step, set the begin time for the polling timer
                    // and increase the _internal step
                    _step_data.wait.begin_time_ms = millis();
                    _internal_step = 1;
                }
                else
                {
                    // wait for the non-blocking timer to expire and finish step
                    if (PollingTimer::timer_finished_ms(_step_data.wait.begin_time_ms, _step_data.wait.wait_ms))
                    {
                        goto next_step;
                    }
                }
            }
            break;

        case HOLD_WEIGHT:
            if (_internal_step == 0)
            {
                // check if weight already in acceptance width. if it is, go to internal step 2, if it's not, go to
                // internal step 1 and set the weight_should_be_greater variable
                if (weight <= _step_data.hold_weight.weight + _step_data.hold_weight.acceptance_width &&
                    weight >= _step_data.hold_weight.weight - _step_data.hold_weight.acceptance_width)
                {
                    // if no wait_ms, then just finish. Otherwise jump to internal_step 2
                    if (_step_data.hold_weight.wait_ms > 0)
                    {

                    }
                    else
                    {
                        _internal_step = 2;
                    }
                }
                else
                {
                    _step_data.hold_weight.weight_should_be_greater = weight <= _step_data.hold_weight.weight;
                    _internal_step = 1;
                }

            }
            
            if (_internal_step == 1)
            {
                if (_step_data.hold_weight.weight_should_be_greater && (weight > _step_data.hold_weight.weight) ||
                   !_step_data.hold_weight.weight_should_be_greater && (weight < _step_data.hold_weight.weight))
                {
                    // goal accomplished and we should head for the next phase
                    // if there is a min_wait, let's set that up. Otherwise finish
                    if (_step_data.hold_weight.wait_ms > 0)
                    {
                        _step_data.hold_weight.begin_time_ms = millis();
                        _internal_step = 2;
                    }
                    else
                    {
                        goto next_step;
                    }
                }
                else
                {
                    // act to get to the weight goal
                    *should_water = _step_data.hold_weight.weight > weight;
                }
            }
            else if (_internal_step == 2)
            {
                if (PollingTimer::timer_finished_ms(_step_data.hold_weight.begin_time_ms, _step_data.hold_weight.wait_ms))
                {
                    // if the timer is over, finish
                    goto next_step;
                }

                // if it's not over, keep the weight within the acceptance width
                if (weight < _step_data.hold_weight.weight - _step_data.hold_weight.acceptance_width)
                {
                    *should_water = true;
                }
            }
            break;

        default:
            ERROR_PRINTFLN("This should be unreachable (%u)", _type);
        }

        return;

        next_step:
        _internal_step = 0;
        *finished = true;
    }
};

#ifndef RUN_SCALE_PROTOCOL_MAX_STEPS
#define RUN_SCALE_PROTOCOL_MAX_STEPS 16
#endif

#define SCALE_PROTOCOL_JSON_STEPS_KEY "s"
#define SCALE_PROTOCOL_JSON_CYCLIC_KEY "c"
#define SCALE_PROTOCOL_JSON_SLOT_KEY "c"

class ScaleProtocol
{
private:
    ScaleProtocolStep _steps[RUN_SCALE_PROTOCOL_MAX_STEPS];
    size_t _n_steps;
    bool _cyclic;
    uint8_t _curr_step = 0;
    uint32_t _curr_cycle = 0;
    
    uint8_t _slot;
    bool _populated = false;

public:
    bool to_json(JsonObject *obj)
    {
        if (!_populated)
        {
            ERROR_PRINTLN("Cannot get json from ScalePRotocol becaues it is not populated");
            return false;
        }

        JsonArray arr = (*obj)[SCALE_PROTOCOL_JSON_STEPS_KEY].to<JsonArray>();
        for (size_t i = 0; i < _n_steps; i++)
        {
            JsonObject sub_obj = arr.add<JsonObject>();
            _steps[sub_obj].to_json(&sub_obj);
        }
        (*obj)[SCALE_PROTOCOL_JSON_CYCLIC_KEY] = _cyclic;
        (*obj)[SCALE_PROTOCOL_JSON_SLOT_KEY] = _slot;
        return true;
    }

    bool from_json(JsonObject *obj)
    {
        if (!(*obj)[SCALE_PROTOCOL_JSON_STEPS_KEY].is<JsonArray>() ||
            !(*obj)[SCALE_PROTOCOL_JSON_CYCLIC_KEY].is<bool>() ||
            !(*obj)[SCALE_PROTOCOL_JSON_SLOT_KEY].is<uint8_t>())
        {
            ERROR_PRINTFLN("Scale protocol json couldn't cast [%s -> JsonArray; %s -> bool; %s -> uint8_t]",
                SCALE_PROTOCOL_JSON_STEPS_KEY, SCALE_PROTOCOL_JSON_CYCLIC_KEY, SCALE_PROTOCOL_JSON_SLOT_KEY);
            _populated = false;
            return false;
        }

        JsonArray arr = (*obj)[SCALE_PROTOCOL_JSON_STEPS_KEY].as<JsonArray>();
        _n_steps = arr.size();
        _cyclic = (*obj)[SCALE_PROTOCOL_JSON_CYCLIC_KEY].as<bool>();
        _slot = (*obj)[SCALE_PROTOCOL_JSON_SLOT_KEY].as<uint8_t>();

        if (_n_steps >= RUN_SCALE_PROTOCOL_MAX_STEPS)
        {
            ERROR_PRINTFLN("Cannot load json for ScaleProtocol with %u steps when %u is the max", _n_steps, RUN_SCALE_PROTOCOL_MAX_STEPS);
            _populated = false;
            return false;
        }

        for (size_t i = 0; i < _n_steps; i++)
        {
            if (!arr[i].is<JsonObject>())
            {
                ERROR_PRINTFLN("Element %u in ScaleProtocol steps array is not JsonObject", i);
                _populated = false;
                return false;
            }
            JsonObject sub_obj = arr[i].as<JsonObject>();
            if (!_steps[i].from_json(&sub_obj))
            {
                ERROR_PRINTFLN("Element %u in ScaleProtocol steps array couldn't be transformed into ScaleProtocolStep", i);
                _populated = false;
                return false;
            }
        }

        _populated = true;
        return true;
    }

    void tick(float weight, bool *should_water, bool *finished_protocol, uint8_t *curr_step=NULL)
    {
        // this method takes in the weight and outputs whether the scale should be watered.
        // this will also keep track of the current step the scale is in

        // if finished a cycle, but not cyclic, protocol has finished
        *finished_protocol = _curr_cycle && !_cyclic;
        *should_water = false;
        if (*finished_protocol)
            return;

        // get previous step
        ScaleProtocolStep *previous_step = NULL;
        size_t n_steps_taken = _curr_cycle * _n_steps + _curr_step;
        if (n_steps_taken > 0)
        {
            previous_step = &_steps[_curr_step > 1 ? _curr_step-1 : _n_steps-1];
        }

        bool finished;
        _steps[_curr_step].run(weight, previous_step, &finished, should_water);

        // if finished step
        if (finished)
        {
            // if end of the cycle
            if (++_curr_step >= _n_steps)
            {
                ++_curr_cycle;
                // check if protocol finished
                if (_curr_cycle && !_cyclic)
                {
                    *finished_protocol = true;
                    *curr_step = _curr_step;
                    return;
                }
                else
                    _curr_step = 0;
            }
        }

        *curr_step = _curr_step;
    }
};

#endif /* _SCALE_PROTOCOL_H_ */