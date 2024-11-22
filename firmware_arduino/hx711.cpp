#include "hx711.h"

#include <limits>
#include <hardware/sync.h>
#include "welfords.h"
#include "debug_helper.h"



bool HX711Calibration::to_json(JsonObject *obj) const
{
    if (!set_offset)
    {
        ERROR_PRINTLN("Can't create json for calibration: offset data flag is false");
        return false;
    }

    (*obj)[HX711_CALIBRATION_JSON_KEY_SLOT] = slot;
    (*obj)[HX711_CALIBRATION_JSON_KEY_OFFSET] = offset;
    (*obj)[HX711_CALIBRATION_JSON_KEY_OFFSET_ERROR] = offset_e;

    if (set_slope)
    {
        (*obj)[HX711_CALIBRATION_JSON_KEY_SLOPE] = slope;
        (*obj)[HX711_CALIBRATION_JSON_KEY_SLOPE_ERROR] = slope_e;
    }
    else
    {
        WARN_PRINTLN("Creating json for calibration without slope data: slope data flag is false");
    }

    return true;
}

bool HX711Calibration::to_json(char *buf, size_t buf_len) const
{
    if (buf_len < HX711_CALIBRATION_JSON_BUF_LEN)
    {
        ERROR_PRINTFLN("Buffer too small. It is %lu but should be at least %lu", buf_len, HX711_CALIBRATION_JSON_BUF_LEN);
        return false;
    }
    
    // set offset
    if (!set_offset)
    {
        ERROR_PRINTLN("Can't create json for calibration: offset data flag is false");
        return false;
    }
    
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();

    if (!to_json(&obj))
    {
        ERROR_PRINTLN("Error in method to_json(JsonDocument&)");
        return false;
    }

    size_t theoretical_length = measureJson(obj);
    if (theoretical_length > buf_len-1)
    {
        ERROR_PRINTFLN("The theoretical length of the JSON (%lu) is bigger than the buffer length (%lu)", theoretical_length, buf_len);
        return false;
    }
    size_t actual_length = serializeJson(obj, buf, buf_len);
    bool res = actual_length >= theoretical_length;
    if (!res)
        ERROR_PRINTFLN("The number of bytes written (%lu) is smaller than the theoretical length of the stringified JSON (%lu)", actual_length, theoretical_length);
    return res;
}

bool HX711Calibration::from_json(JsonObject *obj)
{
    if (!obj)
    {
        ERROR_PRINTLN("Can't load calibration json: obj is not a JsonObject");
        return false;
    }

    // check offset keys exist and are of right type
    if (!(*obj)[HX711_CALIBRATION_JSON_KEY_SLOT].is<uint8_t>() ||
        !(*obj)[HX711_CALIBRATION_JSON_KEY_OFFSET].is<float>() ||
        !(*obj)[HX711_CALIBRATION_JSON_KEY_OFFSET_ERROR].is<float>())
    {
        ERROR_PRINTFLN("Can't load calibration json: slot is not uint8_t ('%s') or offset keys ('%s' and '%s') aren't floats", HX711_CALIBRATION_JSON_KEY_SLOT, HX711_CALIBRATION_JSON_KEY_OFFSET, HX711_CALIBRATION_JSON_KEY_OFFSET_ERROR);
        return false;
    }
    slot = (*obj)[HX711_CALIBRATION_JSON_KEY_SLOT].as<uint8_t>();
    offset = (*obj)[HX711_CALIBRATION_JSON_KEY_OFFSET].as<float>();
    offset_e = (*obj)[HX711_CALIBRATION_JSON_KEY_OFFSET_ERROR].as<float>();
    set_offset = true;

    // check offset keys exist and are of right type
    set_slope = false;
    if (!(*obj)[HX711_CALIBRATION_JSON_KEY_SLOPE].is<float>() ||
        !(*obj)[HX711_CALIBRATION_JSON_KEY_SLOPE_ERROR].is<float>())
    {
        slope = (*obj)[HX711_CALIBRATION_JSON_KEY_SLOPE].as<float>();
        slope_e = (*obj)[HX711_CALIBRATION_JSON_KEY_SLOPE_ERROR].as<float>();
        set_slope = true;
    }

    if (!set_slope)
    {
        WARN_PRINTLN("Couldn't load slope data for calibration");
    }

    return true;
}

bool HX711Calibration::from_json(char *buf, size_t buf_len)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, buf, buf_len-1);
    buf[buf_len-1] = '\0';

    if (error) {
        ERROR_PRINTFLN("Can't load calibration json: deserialization failed with error: '%s'\nThe original content was '%s'", error.c_str(), buf);
        return false;
    }

    // check it's an object and cast it
    JsonObject obj = doc.as<JsonObject>();
    return from_json(&obj);
}

bool HX711Calibration::populated() const
{
    return set_offset || set_slope;
}

inline void HX711::pulse()
{
    // pulse
    digitalWrite(_pin_sck, HIGH);
    sleep_us(HX711_PULSE_DELAY_US);
    digitalWrite(_pin_sck, LOW);
    sleep_us(HX711_PULSE_DELAY_US);
}

HX711::HX711(pin_size_t pin_sck, pin_size_t pin_dout, HX711Gain gain)
: _pin_sck(pin_sck), _pin_dout(pin_dout), _gain(gain)
{}

void HX711::begin()
{
    pinMode(_pin_sck, OUTPUT);
    pinMode(_pin_dout, INPUT);
}

bool HX711::is_ready()
{
    return digitalRead(_pin_dout) == LOW;
}

void HX711::wait_ready()
{
    while (!is_ready())
        sleep_us(HX711_IS_READY_POLLING_DELAY_US);
}

bool HX711::wait_ready_timeout(unsigned long timeout_ms)
{
    const unsigned long start_time_ms = millis();
    unsigned long ms, time_diff = 0;
    while (time_diff < timeout_ms)
    {
        if (is_ready()) return true;
        sleep_us(HX711_IS_READY_POLLING_DELAY_US);

        ms = millis();
        if (ms > start_time_ms)
            time_diff = ms - start_time_ms;
        else
            time_diff = (std::numeric_limits<unsigned long>::max() - start_time_ms) + ms;
    }

    WARN_PRINTLN("HX711 timed out waiting for to be ready");
    return false;
}

bool HX711::read_raw_single(int32_t *raw, uint32_t timeout_ms)
{
    uint32_t ints;
    uint8_t i, j, data[3] = {0}, filler;

    // send ready request
    power_on();

    // wait for ready
    if (timeout_ms > 0)
        if (!wait_ready_timeout(timeout_ms))
        {
            ERROR_PRINTLN("Timeout error in HX711::read_raw_single");
            return false;
        }
    else
        wait_ready();

    // read
    for (i = 0; i < 3; i++)
    {
        for (j = 0; i < 8; i++)
        {
            ints = save_and_disable_interrupts(); 
            pulse();
            restore_interrupts(ints);

            // shift
            data[i] |= digitalRead(_pin_dout) << (7 - j);
        }
    }

    // set correct gain
    ints = save_and_disable_interrupts(); 
    for (i = 0; i < _gain; i++)
    {
        pulse();
    }
    restore_interrupts(ints);

    // to twos complement
    if (data[2] & 0x80)
        filler = 0xFF;
    else
        filler = 0x00;

    (*raw) = static_cast<int32_t>(static_cast<uint32_t>(filler) << 24 |
             static_cast<uint32_t>(data[2]) << 16 |
             static_cast<uint32_t>(data[1]) << 8 |
             static_cast<uint32_t>(data[0]));
    return true;
}

bool HX711::read_raw_stats(uint32_t n, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms)
{
    int32_t raw;
    if (n == 0)
    {
        ERROR_PRINTLN("Can't read stats with n = 0");
        return false;
    }
    else if (n == 1)
    {
        if (!read_raw_single(&raw, timeout_ms))
        {
            ERROR_PRINTLN("Error in HX711::read_raw_stats due to error in HX711::read_raw_single");
            return false;
        }
        (*mean) = static_cast<float>(raw);
        (*stdev) = -1.0;
        WARN_PRINTLN("Using read_raw_stats with n = 1. Use read_raw_single instead");
        return true;
    }

    Welfords::Aggregate agg;
    float temp_mean, temp_stdev;
    for (uint32_t i = 0; i < n; i++)
    {
        if (!read_raw_single(&raw, timeout_ms)) continue;
        Welfords::update(&agg, static_cast<float>(raw));
    }

    if (!Welfords::finalize(&agg, mean, stdev))
    {
        ERROR_PRINTFLN("Couldn't finalize welford's algorithm because there weren't enough samples (%lu)", agg.count);
        return false;
    }
    (*resulting_n) = agg.count;
    return true;
}

bool HX711::read_calib_stats(uint32_t n, HX711Calibration *calib, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms)
{
    float raw_mean, raw_stdev;
    if (!read_raw_stats(n, &raw_mean, &raw_stdev, resulting_n, timeout_ms))
    {
        ERROR_PRINTLN("Error in HX711::read_calib_stats due to error in HX711::read_raw_stats");
        return false;
    }

    (*mean) = calib->slope * raw_mean + calib->offset;
    (*stdev) = sqrt( sq(calib->offset_e) + sq(calib->slope_e)*sq(raw_mean) + sq(calib->slope)*sq(raw_stdev) );

    return true;
}

bool HX711::calib_offset(uint32_t n, HX711Calibration *calib, uint32_t *resulting_n, uint32_t timeout_ms)
{
    float raw_mean, raw_stdev;
    if (!read_raw_stats(n, &raw_mean, &raw_stdev, resulting_n, timeout_ms))
    {
        ERROR_PRINTLN("Error in HX711::calib_offset due to error in HX711::read_raw_stats");
        return false;
    }

    calib->offset = raw_mean;
    calib->offset_e = raw_stdev;
    calib->set_offset = true;

    return true;
}

bool HX711::calib_slope(uint32_t n, float weight, float weight_error, HX711Calibration *calib, uint32_t *resulting_n, uint32_t timeout_ms)
{
    if (!calib->set_offset)
    {
        ERROR_PRINTLN("Cannot calibrate slope when offset is not set");
        return false;
    }
    float raw_mean, raw_stdev;
    if (!read_raw_stats(n, &raw_mean, &raw_stdev, resulting_n, timeout_ms))
    {
        ERROR_PRINTLN("Error in HX711::calib_offset due to error in HX711::read_raw_stats");
        return false;
    }

    calib->slope = (weight - calib->offset) / raw_mean;

    float raw_mean2 = sq(raw_mean);
    float temp = raw_stdev / raw_mean2;
    temp = abs(temp); // this shouldn't be necessary
    calib->slope_e = sqrt( (sq(calib->offset_e) + sq(weight_error)) / raw_mean2 + sq(weight - calib->offset) * abs(temp) );
    calib->set_slope = true;

    return true;
}

void HX711::power_off(bool wait_until_power_off)
{
    digitalWrite(_pin_sck, LOW);
    digitalWrite(_pin_sck, HIGH);
    if (wait_until_power_off)
        sleep_us(HX711_POWER_DOWN_DELAY_US);
}

void HX711::power_on()
{
    digitalWrite(_pin_sck, LOW);
}