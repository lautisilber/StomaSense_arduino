#include "ArduinoJson/Array/JsonArray.hpp"
#include "ArduinoJson/Object/JsonObject.hpp"
#include "ArduinoJson/Document/JsonDocument.hpp"
#ifndef _RUN_H_
#define _RUN_H_

#include "HX711_Mult.h"
#include "debug_helper.h"
#include <ArduinoJson.h>
#include "sd_helper.h"
#include "scale_protocol.h"
#include "algos.h"

struct ScalePosition
{
    uint8_t slot;
    int32_t stepper;
    uint8_t servo;
    uint32_t pump_time_us;
    float pump_intensity;
};

#define RUN_JSON_KEY_SCALE_POSITIONS                "p"

#define RUN_JSON_KEY_SCALE_POSITIONS_SLOT           "l"
#define RUN_JSON_KEY_SCALE_POSITIONS_STEPPER        "t"
#define RUN_JSON_KEY_SCALE_POSITIONS_SERVO          "v"
#define RUN_JSON_KEY_SCALE_POSITIONS_PUMP_TIME_US   "t"
#define RUN_JSON_KEY_SCALE_POSITIONS_PUMP_INTENSITY "i"
#define RUN_JSON_KEY_BME_TIME_MS                    "b"

#define RUN_JSON_KEY_SCALE_PROTOCOLS                 "q"

#define RUN_SAVE_FILE "RUNDATA.JSN"


template <size_t N_SCALES>
class RunData
{
private:
    ScalePosition _scale_positions[N_SCALES];
    ScaleProtocol _scale_protocols[N_SCALES];
    uint32_t _bme_time_ms;
    bool _populated_flag = false;

public:
    RunData()
    {
        static_assert(N_SCALES <= N_MULTIPLEXERS, "Number of scales is greater than the number of multiplexed hx711");
    }

    bool get_slot_data(uint8_t slot, const ScalePosition *pos, const ScaleProtocol *protocol)
    {
        if (slot > N_SCALES)
        {
            ERROR_PRINTFLN("Cannot get slot %u because N_SCALES = %u", slot, N_SCALES);
            return false;
        }
    }

    bool set_data(JsonDocument *doc)
    {
        // check for Scale Positions
        if (!(*doc)[RUN_JSON_KEY_SCALE_POSITIONS].is<JsonArray>())
        {
            ERROR_PRINTFLN("Key for scale positions ('%s') doesn't cointain a JsonArray", RUN_JSON_KEY_SCALE_POSITIONS);
            return false;
        }
        if (!(*doc)[RUN_JSON_KEY_SCALE_PROTOCOLS].is<JsonArray>())
        {
            ERROR_PRINTFLN("Key for scale protocols ('%s') doesn't contain a JsonArray", RUN_JSON_KEY_SCALE_PROTOCOLS);
            return false;
        }

        JsonArray scale_protocols = (*doc)[RUN_JSON_KEY_SCALE_PROTOCOLS].as<JsonArray>();
        if (scale_protocols.size() != N_SCALES)
        {
            ERROR_PRINTFLN("Scale protocols array (key '%s') is size %u when it should be %u", RUN_JSON_KEY_SCALE_PROTOCOLS, scale_protocols.size(), N_SCALES);
            return false;
        }
        for (size_t i = 0; i < N_SCALES; i++)
        {
            JsonObject sub_obj = scale_protocols[i].as<JsonObject>();
            if (!sub_obj)
            {
                ERROR_PRINTFLN("Scale protocols array (key '%s') doesn't contain a JsonObject in index %u", RUN_JSON_KEY_SCALE_PROTOCOLS, i);
                return false;
            }
            if (_scale_protocols[i].from_json(&sub_obj))
            {
                ERROR_PRINTFLN("Scale protocols array failed to read json in index %u", RUN_JSON_KEY_SCALE_PROTOCOLS, i);
                return false;
            }
        }

        JsonArray scale_positions = (*doc)[RUN_JSON_KEY_SCALE_POSITIONS].as<JsonArray>();
        if (scale_positions.size() != N_SCALES)
        {
            ERROR_PRINTFLN("Scale positions array (key '%s') is size %u when it should be %u", RUN_JSON_KEY_SCALE_POSITIONS, scale_positions.size(), N_SCALES);
            return false;
        }
        // check inside Scale Positions
        for (JsonVariant var : scale_positions)
        {
            if (!var.is<JsonObject>())
            {
                ERROR_PRINTLN("Not all elements in scale positions array are objects");
                return false;
            }
            JsonObject obj = var.as<JsonObject>();
            if (
                !obj[RUN_JSON_KEY_SCALE_POSITIONS_SLOT].is<uint8_t>() ||
                !obj[RUN_JSON_KEY_SCALE_POSITIONS_STEPPER].is<int32_t>() ||
                !obj[RUN_JSON_KEY_SCALE_POSITIONS_SERVO].is<uint8_t>() ||
                !obj[RUN_JSON_KEY_SCALE_POSITIONS_PUMP_TIME_US].is<uint32_t>() ||
                !obj[RUN_JSON_KEY_SCALE_POSITIONS_PUMP_INTENSITY].is<uint32_t>()
            )
            {
                ERROR_PRINTFLN("One of the following keys inside scale position couldn't be cast: [%s -> uint8_t; %s -> int32_t; %s -> uint8_t; %s -> uint32_t; %s -> uint32_t]",
                               RUN_JSON_KEY_SCALE_POSITIONS_SLOT, RUN_JSON_KEY_SCALE_POSITIONS_STEPPER, RUN_JSON_KEY_SCALE_POSITIONS_SERVO, RUN_JSON_KEY_SCALE_POSITIONS_PUMP_TIME_US, RUN_JSON_KEY_SCALE_POSITIONS_PUMP_INTENSITY);
                return false;
            }
        }
        // check bme key and type
        if (!(*doc)[RUN_JSON_KEY_BME_TIME_MS].is<uint32_t>())
        {
            ERROR_PRINTFLN("The key '%s' cannot be cast to type uint32_t", RUN_JSON_KEY_BME_TIME_MS);
            return false;
        }

        // get data
        size_t i = 0;
        for (size_t i = 0; i < N_SCALES; ++i)
        {
            JsonObject obj = scale_positions[i].as<JsonObject>();
            _scale_positions[i].slot = obj[RUN_JSON_KEY_SCALE_POSITIONS_SLOT].as<uint8_t>();
            _scale_positions[i].stepper = obj[RUN_JSON_KEY_SCALE_POSITIONS_STEPPER].as<int32_t>();
            _scale_positions[i].servo = obj[RUN_JSON_KEY_SCALE_POSITIONS_SERVO].as<uint8_t>();
            _scale_positions[i].pump_time_us = obj[RUN_JSON_KEY_SCALE_POSITIONS_PUMP_TIME_US].as<uint32_t>();
            _scale_positions[i].pump_intensity = obj[RUN_JSON_KEY_SCALE_POSITIONS_PUMP_INTENSITY].as<uint32_t>();
        }
        _bme_time_ms = (*doc)[RUN_JSON_KEY_BME_TIME_MS].as<uint32_t>();
        _populated_flag = true;

        // sort according to slots
        int16_t slots[N_SCALES]
        sort_with_index_array

        return true;
    }

    bool set_data(const char *json_str)
    {
        // get a json string and extract the run data from it
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, json_str);
        if (error)
        {
            ERROR_PRINTFLN("Coundln't deserialize Run data with error: '%s'", error.c_str());
            return false;
        }

        return set_data(&doc);
    }

    bool set_data(Stream *stream)
    {
        // get a json string and extract the run data from it
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, stream);
        if (error)
        {
            ERROR_PRINTFLN("Coundln't deserialize Run data with error: '%s'", error.c_str());
            return false;
        }

        return set_data(&doc);
    }

    bool get_data(JsonObject *obj)
    {
        if (!_populated_flag)
        {
            return false;
        }

        JsonArray scale_positions_arr = (*obj)[RUN_JSON_KEY_SCALE_POSITIONS].to<JsonArray>();
        for (uint8_t i = 0; i < N_SCALES; ++i)
        {
            JsonObject sub_obj = scale_positions_arr.add<JsonObject>();
            sub_obj[RUN_JSON_KEY_SCALE_POSITIONS_SLOT] = _scale_positions[i].slot;
            sub_obj[RUN_JSON_KEY_SCALE_POSITIONS_STEPPER] = _scale_positions[i].stepper;
            sub_obj[RUN_JSON_KEY_SCALE_POSITIONS_SERVO] = _scale_positions[i].servo;
            sub_obj[RUN_JSON_KEY_SCALE_POSITIONS_PUMP_TIME_US] = _scale_positions[i].pump_time_us;
            sub_obj[RUN_JSON_KEY_SCALE_POSITIONS_PUMP_INTENSITY] = _scale_positions[i].pump_intensity;
        }
        JsonArray scale_protocols_arr = (*obj)[RUN_JSON_KEY_SCALE_PROTOCOLS].to<JsonArray>();
        for (uint8_t i = 0; i < N_SCALES; ++i)
        {
            JsonObject sub_obj = scale_protocols_arr.add<JsonObject>();
            if (!_scale_protocols[i].to_json(&sub_obj))
            {
                WARN_PRINTFLN("Couldn't save scale protocol index %u", i);
            }
        }
        (*obj)[RUN_JSON_KEY_BME_TIME_MS] = _bme_time_ms;
        return true;
    }

    bool save()
    {
        JsonDocument doc;
        JsonObject obj = doc.to<JsonObject>();

        if (!get_data(&obj))
        {
            ERROR_PRINTLN("Can't save Run data because it is not populated");
            return false;
        }

        size_t json_str_size;
        bool res = true;
        File file;
        if (!SD_Helper::open_write(&file, RUN_SAVE_FILE))
        {
            ERROR_PRINTLN("Couldn't save Run data: Couldn't open file");
            res = false;
            goto close_file_and_return;
        }
        json_str_size = measureJson(obj);
        if (serializeJson(obj, file) < json_str_size)
        {
            ERROR_PRINTLN("Didn't write enough chars to the Run data save file while saving");
            res = false;
            goto close_file_and_return;
        }

        close_file_and_return:
        if (!SD_Helper::close(&file))
        {
            WARN_PRINTLN("Error closing Run data file after saving");
        }
        return res;
    }

    bool load()
    {
        bool res = true;
        File file;
        if (!SD_Helper::open_read(&file, RUN_SAVE_FILE))
        {
            ERROR_PRINTLN("Couldn't load Run data: Couldn't open file");
            res = false;
        }
        else
            res = set_data(&file);

        if (!SD_Helper::close(&file))
            WARN_PRINTLN("Error closing Run data file after saving");
        if (!res)
        {
            ERROR_PRINTLN("Couldn't save Run data");
        }
        return res;
    }
};



#endif /* _RUN_H_ */