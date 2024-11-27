#include "scale_position.h"
#include "debug_helper.h"


bool ScalePosition::from_json(JsonObject *obj)
{
    if (!(*obj)[SCALE_POSITION_JSON_KEY_SLOT].is<uint8_t>() ||
        !(*obj)[SCALE_POSITION_JSON_KEY_STEPPER].is<int32_t>() ||
        !(*obj)[SCALE_POSITION_JSON_KEY_SERVO].is<uint8_t>() ||
        !(*obj)[SCALE_POSITION_JSON_KEY_PUMP_TIME_US].is<uint32_t>() ||
        !(*obj)[SCALE_POSITION_JSON_KEY_PUMP_INTENSITY].is<uint8_t>())
    {
        ERROR_PRINTFLN("One of the following keys inside scale position couldn't be cast: [%s -> uint8_t; %s -> int32_t; %s -> uint8_t; %s -> uint32_t; %s -> uint8_t]",
                            SCALE_POSITION_JSON_KEY_SLOT, SCALE_POSITION_JSON_KEY_STEPPER, SCALE_POSITION_JSON_KEY_SERVO, SCALE_POSITION_JSON_KEY_PUMP_TIME_US, SCALE_POSITION_JSON_KEY_PUMP_INTENSITY);
            return false;
    }

    uint8_t temp_slot = (*obj)[SCALE_POSITION_JSON_KEY_SLOT].as<uint8_t>();
    if (temp_slot >= N_MULTIPLEXERS)
    {
        ERROR_PRINTFLN("The scale position had a slot of value %u when max slot is %u\n", temp_slot, N_MULTIPLEXERS-1);
        return false;
    }
    uint8_t temp_servo = (*obj)[SCALE_POSITION_JSON_KEY_SERVO].as<uint8_t>();
    if (temp_servo < SERVO_MIN_ANGLE || temp_servo > SERVO_MAX_ANGLE)
    {
        ERROR_PRINTFLN("The scale position had a servo of value %u when is should be in the range [%u, %u]\n", temp_servo, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
        return false;
    }
    uint8_t temp_pump_intensity = (*obj)[SCALE_POSITION_JSON_KEY_SERVO].as<uint8_t>();
    if (temp_pump_intensity > 100)
    {
        ERROR_PRINTFLN("The scale position had a pump_intensity of value %u when max pump_intensity is 100\n", temp_pump_intensity);
        return false;
    }

    slot = temp_slot;
    stepper = (*obj)[SCALE_POSITION_JSON_KEY_STEPPER].as<int32_t>();
    servo = temp_servo;
    pump_time_us = (*obj)[SCALE_POSITION_JSON_KEY_PUMP_TIME_US].as<uint32_t>();
    pump_intensity = temp_pump_intensity;

    _init_flag = true;
    return true;
}


bool ScalePosition::to_json(JsonObject *obj) const
{
    if (!_init_flag)
    {
        ERROR_PRINTLN("Cannot get json from ScalePosition because _init_flag is false");
        return false;
    }

    (*obj)[SCALE_POSITION_JSON_KEY_SLOT] = slot;
    (*obj)[SCALE_POSITION_JSON_KEY_STEPPER] = stepper;
    (*obj)[SCALE_POSITION_JSON_KEY_SERVO] = servo;
    (*obj)[SCALE_POSITION_JSON_KEY_PUMP_TIME_US] = pump_time_us;
    (*obj)[SCALE_POSITION_JSON_KEY_PUMP_INTENSITY] = pump_intensity;

    return true;
}