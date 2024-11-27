#ifndef _SCALE_POSITION_H_
#define _SCALE_POSITION_H_

#include <Arduino.h>
#include <ArduinoJson.h>

#include "defs.h"

#define SCALE_POSITION_JSON_KEY_SLOT           "l"
#define SCALE_POSITION_JSON_KEY_STEPPER        "t"
#define SCALE_POSITION_JSON_KEY_SERVO          "v"
#define SCALE_POSITION_JSON_KEY_PUMP_TIME_US   "t"
#define SCALE_POSITION_JSON_KEY_PUMP_INTENSITY "i"

class ScalePosition
{
private:
    bool _init_flag = false;
public:
    uint8_t slot;
    int32_t stepper;
    uint8_t servo;
    uint32_t pump_time_us;
    float pump_intensity;

    bool from_json(JsonObject *obj);
    bool to_json(JsonObject *obj) const;
};


#endif /* _SCALE_POSITION_H_ */