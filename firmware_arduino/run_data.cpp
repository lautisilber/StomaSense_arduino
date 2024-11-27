#include "run_data.h"

#include "HX711_Mult.h"
#include "sd_helper.h"
#include "defs.h"
#include "debug_helper.h"

// RunData::RunData()
// {
//     // static_assert(N_SCALES == N_MULTIPLEXERS, "Number of scales is greater than the number of multiplexed hx711");
// }

const ScalePosition *RunData::get_slot_position(uint8_t slot) const
{
    if (slot > N_MULTIPLEXERS)
    {
        ERROR_PRINTFLN("Cannot get slot %u because N_MULTIPLEXERS = %u", slot, N_MULTIPLEXERS);
        return NULL;
    }
    return (const ScalePosition *)&_scale_positions[slot];
}

ScaleProtocol *RunData::get_slot_protocol(uint8_t slot)
{
    if (slot > N_MULTIPLEXERS)
    {
        ERROR_PRINTFLN("Cannot get slot %u because N_MULTIPLEXERS = %u", slot, N_MULTIPLEXERS);
        return NULL;
    }
    return &_scale_protocols[slot];
}

bool RunData::get_slot_data(uint8_t slot, const ScalePosition *pos, ScaleProtocol *protocol)
{
    if (slot > N_MULTIPLEXERS)
    {
        ERROR_PRINTFLN("Cannot get slot %u because N_MULTIPLEXERS = %u", slot, N_MULTIPLEXERS);
        return false;
    }
    pos = (const ScalePosition *)&_scale_positions[slot];
    protocol = &_scale_protocols[slot];
    return true;
}

bool RunData::set_data(JsonDocument *doc)
{
    // cast to json object
    JsonObject obj = doc->to<JsonObject>();
    if (!obj)
    {
        ERROR_PRINTLN("Couldn't set data of RunData because doc wasn't of type JsonObject");
        return false;
    }

    _init_flag = false;
    bool scales_in_use[N_MULTIPLEXERS];
    memset(_scales_in_use, false, N_MULTIPLEXERS * sizeof(bool));
    memset(scales_in_use, false, N_MULTIPLEXERS * sizeof(bool));

    // check for Scale Positions
    JsonArray scale_positions = obj[RUN_JSON_KEY_SCALE_POSITIONS].as<JsonArray>();
    if (!scale_positions)
    {
        ERROR_PRINTFLN("Key for scale positions ('%s') doesn't cointain a JsonArray", RUN_JSON_KEY_SCALE_POSITIONS);
        return false;
    }
    if (scale_positions.size() > N_MULTIPLEXERS)
    {
        ERROR_PRINTFLN("Scale positions array (key '%s') is size %u when it should be %u", RUN_JSON_KEY_SCALE_POSITIONS, scale_positions.size(), N_MULTIPLEXERS);
        return false;
    }

    for (size_t i = 0; i < scale_positions.size(); i++)
    {
        JsonObject sub_obj = scale_positions[i].as<JsonObject>();
        if (!obj)
        {
            ERROR_PRINTFLN("Scale positions array (key '%s') doesn't contain a JsonObject in index %u", RUN_JSON_KEY_SCALE_POSITIONS, i);
            return false;
        }
        if (!sub_obj[SCALE_POSITION_JSON_KEY_SLOT].is<uint8_t>())
        {
            ERROR_PRINTFLN("Scale positions array element %u didn't contain a slot key (%s) of type uint8_t", i, RUN_JSON_KEY_SCALE_POSITIONS);
            return false;
        }
        uint8_t slot = sub_obj[SCALE_POSITION_JSON_KEY_SLOT].as<uint8_t>();
        if (slot >= N_MULTIPLEXERS)
        {
            ERROR_PRINTFLN("Scale positions array element %u contained a slot key with value %u, when max slot is %u", slot, N_MULTIPLEXERS-1);
            return false;
        }
        if (!_scale_positions[slot].from_json(&obj))
        {
            ERROR_PRINTFLN("Scale positions array failed to read json in index %u (slot %u)", RUN_JSON_KEY_SCALE_POSITIONS, i, slot);
            return false;
        }
        _scales_in_use[slot] = true;
    }

    JsonArray scale_protocols = (*doc)[RUN_JSON_KEY_SCALE_PROTOCOLS].as<JsonArray>();
    if (!scale_protocols)
    {
        ERROR_PRINTFLN("Key for scale protocols ('%s') doesn't cointain a JsonArray", RUN_JSON_KEY_SCALE_PROTOCOLS);
        return false;
    }
    if (scale_protocols.size() > N_MULTIPLEXERS)
    {
        ERROR_PRINTFLN("Scale protocols array (key '%s') is size %u when it should be %u", RUN_JSON_KEY_SCALE_PROTOCOLS, scale_protocols.size(), N_MULTIPLEXERS);
        return false;
    }
    for (size_t i = 0; i < scale_protocols.size(); i++)
    {
        JsonObject sub_obj = scale_protocols[i].as<JsonObject>();
        if (!obj)
        {
            ERROR_PRINTFLN("Scale protocols array (key '%s') doesn't contain a JsonObject in index %u", RUN_JSON_KEY_SCALE_PROTOCOLS, i);
            return false;
        }
        if (!sub_obj[SCALE_PROTOCOL_JSON_SLOT_KEY].is<uint8_t>())
        {
            ERROR_PRINTFLN("Scale protocols array element %u didn't contain a slot key (%s) of type uint8_t", i, RUN_JSON_KEY_SCALE_PROTOCOLS);
            return false;
        }
        uint8_t slot = sub_obj[SCALE_PROTOCOL_JSON_SLOT_KEY].as<uint8_t>();
        if (slot >= N_MULTIPLEXERS)
        {
            ERROR_PRINTFLN("Scale protocols array element %u contained a slot key with value %u, when max slot is %u", slot, N_MULTIPLEXERS-1);
            return false;
        }
        if (!_scale_protocols[slot].from_json(&obj))
        {
            ERROR_PRINTFLN("Scale protocols array failed to read json in index %u (slot %u)", RUN_JSON_KEY_SCALE_PROTOCOLS, i, slot);
            return false;
        }
        scales_in_use[slot] = true;
    }

    // the same slots for position and protocols must be set!
    if (memcmp(_scales_in_use, scales_in_use, N_MULTIPLEXERS * sizeof(bool)) != 0)
    {
        ERROR_PRINTLN("Not the same slots for positions and protocols were loaded");
        return false;
    }

    // check bme key and type
    if (!obj[RUN_JSON_KEY_BME_TIME_MS].is<uint32_t>())
    {
        ERROR_PRINTFLN("The key '%s' cannot be cast to type uint32_t", RUN_JSON_KEY_BME_TIME_MS);
        return false;
    }

    _init_flag = true;
    return true;
}

bool RunData::set_data(const char *json_str)
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

bool RunData::set_data(Stream *stream)
{
    // get a json string and extract the run data from it
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, *stream);
    if (error)
    {
        ERROR_PRINTFLN("Coundln't deserialize Run data with error: '%s'", error.c_str());
        return false;
    }

    return set_data(&doc);
}

bool RunData::get_data(JsonObject *obj) const
{
    if (!_init_flag)
    {
        ERROR_PRINTLN("The _init_flag was set to false");
        return false;
    }

    JsonArray scale_positions_arr = (*obj)[RUN_JSON_KEY_SCALE_POSITIONS].to<JsonArray>();
    for (uint8_t i = 0; i < N_MULTIPLEXERS; ++i)
    {
        if (!_scales_in_use[i]) continue;
        JsonObject sub_obj = scale_positions_arr.add<JsonObject>();
        if (!_scale_protocols[i].to_json(&sub_obj))
        {
            ERROR_PRINTFLN("Couldn't get scale protocol index %u", i);
            return false;
        }
    }
    JsonArray scale_protocols_arr = (*obj)[RUN_JSON_KEY_SCALE_PROTOCOLS].to<JsonArray>();
    for (uint8_t i = 0; i < N_MULTIPLEXERS; ++i)
    {
        if (!_scales_in_use[i]) continue;
        JsonObject sub_obj = scale_protocols_arr.add<JsonObject>();
        if (!_scale_protocols[i].to_json(&sub_obj))
        {
            ERROR_PRINTFLN("Couldn't get scale protocol index %u", i);
            return false;
        }
    }
    (*obj)[RUN_JSON_KEY_BME_TIME_MS] = _bme_time_ms;
    return true;
}

bool RunData::save() const
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

bool RunData::load()
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
