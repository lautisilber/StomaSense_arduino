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
    void to_json(JsonObject *obj) const;
    bool from_json(JsonObject *obj);

    void run(float weight, ScaleProtocolStep *previous_step, bool *finished, bool *should_water);
};

#ifndef RUN_SCALE_PROTOCOL_MAX_STEPS
#define RUN_SCALE_PROTOCOL_MAX_STEPS 16
#endif

#define SCALE_PROTOCOL_JSON_STEPS_KEY "s"
#define SCALE_PROTOCOL_JSON_CYCLIC_KEY "c"
#define SCALE_PROTOCOL_JSON_SLOT_KEY "l"

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
    bool to_json(JsonObject *obj) const;
    bool from_json(JsonObject *obj);

    void tick(float weight, bool *should_water, bool *finished_protocol, uint8_t *curr_step=NULL);
};

#endif /* _SCALE_PROTOCOL_H_ */