#include "hx711_mult.h"
#include "debug_helper.h"
#include "sd_helper.h"
#include "algos.h"

static const bool multiplexer_configs[N_MULTIPLEXERS][N_MULTIPLEXER_PINS] = {
    {0, 0, 0, 0},
    {0, 1, 0, 0},
    {0, 0, 1, 0},
    {0, 1, 1, 0},
    {0, 0, 0, 1},
    {0, 1, 0, 1},
    {0, 0, 1, 1},
    {0, 1, 1, 1},
    {1, 0, 0, 0},
    {1, 1, 0, 0},
    {1, 0, 1, 0},
    {1, 1, 1, 0},
    {1, 0, 0, 1},
    {1, 1, 0, 1},
    {1, 0, 1, 1},
    {1, 1, 1, 1}
};

inline bool is_slot_valid(uint8_t slot)
{
    return slot < N_MULTIPLEXERS;
}

void HX711_Mult::_set_slot(uint8_t slot)
{
    if (_curr_slot == slot) return;
    digitalWrite(_mult_pin_1, multiplexer_configs[slot][0]);
    digitalWrite(_mult_pin_2, multiplexer_configs[slot][1]);
    digitalWrite(_mult_pin_3, multiplexer_configs[slot][2]);
    digitalWrite(_mult_pin_4, multiplexer_configs[slot][3]);
    _curr_slot = slot;
}

HX711_Mult::HX711_Mult(pin_size_t mult_pin_1, pin_size_t mult_pin_2, pin_size_t mult_pin_3, pin_size_t mult_pin_4,
    pin_size_t pin_sck, pin_size_t pin_dout, HX711Gain gain)
: _hx(pin_sck, pin_dout, gain), _mult_pin_1(mult_pin_1), _mult_pin_2(mult_pin_2), _mult_pin_3(mult_pin_3), _mult_pin_4(mult_pin_4)
{}

void HX711_Mult::begin()
{
    _hx.begin();
    pinMode(_mult_pin_1, OUTPUT);
    pinMode(_mult_pin_2, OUTPUT);
    pinMode(_mult_pin_3, OUTPUT);
    pinMode(_mult_pin_4, OUTPUT);
}

void HX711_Mult::set_slot(uint8_t slot)
{
    if (!is_slot_valid(slot)) return;
    _set_slot(slot);
}

bool HX711_Mult::read_raw_single(uint8_t slot, int32_t *raw, uint32_t timeout_ms)
{
    if (!is_slot_valid(slot))
    {
        ERROR_PRINTFLN("Slot %u is invalid. Slot should be in range [0,%u]", N_MULTIPLEXERS-1);
        return false;
    }
    _set_slot(slot);
    bool res = _hx.read_raw_single(raw, timeout_ms);
    if (!res)
    {
        ERROR_PRINTLN("Error in HX711_Mult::read_raw_single due to error in HX711::read_raw_single");
    }
    return res;
}

bool HX711_Mult::read_raw_stats(uint8_t slot, uint32_t n, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms)
{
    if (!is_slot_valid(slot))
    {
        ERROR_PRINTFLN("Slot %u is invalid. Slot should be in range [0,%u]", N_MULTIPLEXERS-1);
        return false;
    }
    _set_slot(slot);
    bool res = _hx.read_raw_stats(n, mean, stdev, resulting_n, timeout_ms);
    if (!res)
    {
        ERROR_PRINTLN("Error in HX711_Mult::read_raw_stats due to error in HX711::read_raw_stats");
    }
    return res;
}

bool HX711_Mult::read_calib_stats(uint8_t slot, uint32_t n, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms)
{
    if (!is_slot_valid(slot))
    {
        ERROR_PRINTFLN("Slot %u is invalid. Slot should be in range [0,%u]", N_MULTIPLEXERS-1);
        return false;
    }
    HX711Calibration *c = &_calibs[slot];
    if (!c->set_offset || !c->set_slope)
    {
        ERROR_PRINTFLN("Cannot call HX711_Mult::read_calib_stats when offset or slope is not set (slot %u)", slot);
        return false;
    }
    _set_slot(slot);
    bool res = _hx.read_calib_stats(n, c, mean, stdev, resulting_n, timeout_ms);
    if (!res)
    {
        ERROR_PRINTLN("Error in HX711_Mult::read_calib_stats due to error in HX711::read_calib_stats");
    }
    return res;
}

bool HX711_Mult::calib_offset(uint8_t slot, uint32_t n, uint32_t *resulting_n, uint32_t timeout_ms)
{
    if (!is_slot_valid(slot))
    {
        ERROR_PRINTFLN("Slot %u is invalid. Slot should be in range [0,%u]", N_MULTIPLEXERS-1);
        return false;
    }
    _set_slot(slot);
    bool res = _hx.calib_offset(n, &_calibs[slot], resulting_n, timeout_ms);
    if (!res)
    {
        ERROR_PRINTLN("Error in HX711_Mult::calib_offset due to error in HX711::calib_offset");
    }
    return res;
}

bool HX711_Mult::calib_slope(uint8_t slot, uint32_t n, float weight, float weight_error, uint32_t *resulting_n, uint32_t timeout_ms)
{
    if (!is_slot_valid(slot))
    {
        ERROR_PRINTFLN("Slot %u is invalid. Slot should be in range [0,%u]", N_MULTIPLEXERS-1);
        return false;
    }
    _set_slot(slot);
    bool res = _hx.calib_slope(n, weight, weight_error, &_calibs[slot], resulting_n, timeout_ms);
    if (!res)
    {
        ERROR_PRINTLN("Error in HX711_Mult::calib_slope due to error in HX711::calib_slope");
    }
    return res;
}

bool HX711_Mult::power_down(uint8_t slot, bool wait_until_power_off)
{
    if (!is_slot_valid(slot))
    {
        ERROR_PRINTFLN("Slot %u is invalid. Slot should be in range [0,%u]", N_MULTIPLEXERS-1);
        return false;
    }
    _hx.power_off(wait_until_power_off);
    return true;
}

bool HX711_Mult::power_up(uint8_t slot)
{
    if (!is_slot_valid(slot))
    {
        ERROR_PRINTFLN("Slot %u is invalid. Slot should be in range [0,%u]", N_MULTIPLEXERS-1);
        return false;
    }
    _hx.power_on();
    return true;
}


bool HX711_Mult::_load_calibration(JsonDocument *doc)
{
    JsonArray arr = doc->as<JsonArray>();
    if (!arr)
    {
        ERROR_PRINTLN("Couldn't load calibrations: document wasn't a JsonArray");
        return false;
    }

    // if (arr.size() != N_MULTIPLEXERS)
    // {
    //     ERROR_PRINTFLN("Couldn't load calibrations: the save file has %u entries but %u were expected", arr.size(), N_MULTIPLEXERS);
    //     return false;
    // }

    // // validate
    // for (size_t i = 0; i < N_MULTIPLEXERS; ++i)
    // {
    //     JsonObject obj = arr[i].as<JsonObject>();
    //     if (!obj)
    //     {
    //         ERROR_PRINTLN("JsonVariant wasn't a JsonObject");
    //         return false;
    //     }
    // }

    // // write validated data
    // for (size_t i = 0; i < N_MULTIPLEXERS; ++i)
    // {
    //     JsonObject obj = arr[i].as<JsonObject>();
    //     if (!_calibs[i].from_json(&obj))
    //     {
    //         ERROR_PRINTFLN("Couldn't get calib from json for slot %u", i);
    //         return false;
    //     }
    // }

    for (size_t i = 0; i < arr.size(); i++)
    {
        if (i == N_MULTIPLEXERS)
        {
            WARN_PRINTLN("Stopped loading calibration array elements to avoid buffer overflow");
            break;
        }
        JsonObject obj = arr[i].as<JsonObject>();
        if (!obj)
        {
            WARN_PRINTFLN("The element %lu of the calibration array couldn't be loaded because it wasn't a JsonObject", i);
            continue;
        }
        if (!_calibs[i].from_json(&obj))
        {
            WARN_PRINTFLN("Couldn't get calib from json for slot %u", i);
            continue;
        }
    }

    // get map_calibs_slots and validate slot values
    int8_t map_calibs_slots[N_MULTIPLEXERS];
    memset(map_calibs_slots, -1, N_MULTIPLEXERS);
    for (size_t i = 0; i < N_MULTIPLEXERS; i++)
    {
        if (!_calibs[i].populated()) break;
        if (_calibs[i].slot > N_MULTIPLEXERS)
        {
            WARN_PRINTFLN("Calibration number %lu has slot %u, but there are %u slots", i, _calibs[i].slot, N_MULTIPLEXERS);
            // dehabilitate this particular calib
            _calibs[i].set_offset = false;
            _calibs[i].set_slope = false;
        }
        else
        {
            map_calibs_slots[i] = _calibs[i].slot;
        }
    }

    // sort _calibs to match their slot
    sort_with_index_array<HX711Calibration, int8_t, N_MULTIPLEXERS>(_calibs, map_calibs_slots);

    return true;
}

bool HX711_Mult::load_calibration()
{
    File file;

    if (!SD_Helper::open_read(&file, HX711_SAVEFILE))
    {
        SD_Helper::close(&file);
        ERROR_PRINTLN("HX711: Couldn't read file for reading");
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    SD_Helper::close(&file);

    if (error)
    {
        ERROR_PRINTFLN("Can't load calibrations json: deserialization failed with error: '%s'", error.c_str());
        return false;
    }

    return _load_calibration(&doc);
}

bool HX711_Mult::load_calibration(const char *json)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error)
    {
        ERROR_PRINTFLN("Can't load calibrations json: deserialization failed with error: '%s'", error.c_str());
        return false;
    }

    return _load_calibration(&doc);
}

bool HX711_Mult::save_calibration()
{
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (size_t i = 0; i < N_MULTIPLEXERS; i++)
    {
        if (!_calibs[i].populated()) continue;
        JsonDocument sub_doc;
        JsonObject obj = sub_doc.to<JsonObject>();
        _calibs[i].to_json(&obj);
        arr.add(obj);
    }


    File file;

    if (!SD_Helper::open_write(&file, HX711_SAVEFILE))
    {
        SD_Helper::close(&file);
        ERROR_PRINTLN("HX711_Mult: Couldn't write file");
        return false;
    }

    size_t min_size = measureJson(arr);
    size_t actual_size = serializeJson(arr, file);
    SD_Helper::close(&file);

    if (min_size < actual_size)
    {
        ERROR_PRINTFLN("Couldn't save calibrations: written %u bytes when the json was %u bytes long", actual_size, min_size);
        return false;
    }

    return true;
}