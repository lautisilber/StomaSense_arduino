#ifndef _RUN_H_
#define _RUN_H_

#include "HX711_Mult.h"
#include "debug_helper.h"
#include <ArduinoJson.h>
#include "sd_helper.h"

struct ScalePosition
{
    int32_t stepper;
    uint8_t servo;
    uint32_t pump_time_us;
    float pump_intensity;
};

#define RUN_JSON_KEY_SCALE_POSITIONS                "sp"
#define RUN_JSON_KEY_SCALE_POSITIONS_STEPPER        "st"
#define RUN_JSON_KEY_SCALE_POSITIONS_SERVO          "sv"
#define RUN_JSON_KEY_SCALE_POSITIONS_PUMP_TIME_US   "pt"
#define RUN_JSON_KEY_SCALE_POSITIONS_PUMP_INTENSITY "pi"
#define RUN_JSON_KEY_BME_TIME_MS                    "bm"

#define RUN_SAVE_FILE "RUNDATA.JSN"

template <size_t N_SCALES>
class RunData
{
private:
    ScalePosition _scale_positions[N_SCALES];
    uint32_t _bme_time_ms;
    bool _populated_flag = false;

public:
    RunData()
    {
        static_assert(N_SCALES == N_MULTIPLEXERS, "Number of scales doesn't match the number of multiplexed hx711");
    }

    bool set_data(JsonDocument *doc)
    {
        // check for Scale Positions ("sp")
        if (!(*doc)[RUN_JSON_KEY_SCALE_POSITIONS].is<JsonArray>())
        {
            ERROR_PRINTFLN("Key for scale positions ('%s') doesn't cointain an array", RUN_JSON_KEY_SCALE_POSITIONS);
            return false;
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
                !obj[RUN_JSON_KEY_SCALE_POSITIONS_STEPPER].is<int32_t>() ||
                !obj[RUN_JSON_KEY_SCALE_POSITIONS_SERVO].is<uint8_t>() ||
                !obj[RUN_JSON_KEY_SCALE_POSITIONS_PUMP_TIME_US].is<uint32_t>() ||
                !obj[RUN_JSON_KEY_SCALE_POSITIONS_PUMP_INTENSITY].is<uint32_t>()
            )
            {
                ERROR_PRINTFLN("One of the following keys inside scale position couldn't be cast: [%s -> int32_t; %s -> uint8_t; %s -> uint32_t; %s -> uint32_t]",
                               RUN_JSON_KEY_SCALE_POSITIONS_STEPPER, RUN_JSON_KEY_SCALE_POSITIONS_SERVO, RUN_JSON_KEY_SCALE_POSITIONS_PUMP_TIME_US, RUN_JSON_KEY_SCALE_POSITIONS_PUMP_INTENSITY);
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
            JsonObject obj = var[i].as<JsonObject>();
            _scale_positions[i].stepper = obj[RUN_JSON_KEY_SCALE_POSITIONS_STEPPER];
            _scale_positions[i].servo = obj[RUN_JSON_KEY_SCALE_POSITIONS_SERVO];
            _scale_positions[i].pump_time_us = obj[RUN_JSON_KEY_SCALE_POSITIONS_PUMP_TIME_US];
            _scale_positions[i].pump_intensity = obj[RUN_JSON_KEY_SCALE_POSITIONS_PUMP_INTENSITY];
        }
        _bme_time_ms = (*doc)[RUN_JSON_KEY_BME_TIME_MS].as<uint32_t>();
        _populated_flag = true;
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

    bool save()
    {
        if (!_populated_flag)
        {
            ERORR_PRINTLN("Can't save Run data because it is not populated");
            return false;
        }

        JsonDocument doc;
        
        JsonArray arr = doc[RUN_JSON_KEY_SCALE_POSITIONS].to<JsonArray>();
        for (uint8_t i = 0; i < N_SCALES; ++i)
        {
            JsonObject obj = arr.add<JsonObject>();
            obj[RUN_JSON_KEY_SCALE_POSITIONS_STEPPER] = _scale_positions[i].stepper;
            obj[RUN_JSON_KEY_SCALE_POSITIONS_SERVO] = _scale_positions[i].servo;
            obj[RUN_JSON_KEY_SCALE_POSITIONS_PUMP_TIME_US] = _scale_positions[i].pump_time_us;
            obj[RUN_JSON_KEY_SCALE_POSITIONS_PUMP_INTENSITY] = _scale_positions[i].pump_intensity;
        }
        doc[RUN_JSON_KEY_BME_TIME_MS] = _bme_time_ms;

        bool res = true;
        File file;
        if (!SD_Helper::open_write(&file, RUN_SAVE_FILE))
        {
            ERROR_PRINTLN("Couldn't save Run data: Couldn't open file");
            res = false;
            goto close_file_and_return;
        }
        size_t json_str_size = measureJson(doc);
        if (serializeJson(doc, file) < json_str_size)
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