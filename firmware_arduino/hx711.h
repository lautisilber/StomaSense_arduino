#include <sys/_stdint.h>
#ifndef _HX711_H_
#define _HX711_H_

#include <Arduino.h>
#include <ArduinoJson.h>
#include "debug_helper.h"

// inspired by https://github.com/bogde/HX711

#define HX711_PULSE_DELAY_US 1
#define HX711_IS_READY_POLLING_DELAY_US 1000
#define HX711_POWER_DOWN_DELAY_US 60

#define HX711_CALIBRATION_JSON_BUF_LEN          96
#define HX711_CALIBRATION_JSON_KEY_OFFSET       "o"
#define HX711_CALIBRATION_JSON_KEY_OFFSET_ERROR "oe"
#define HX711_CALIBRATION_JSON_KEY_SLOPE        "s"
#define HX711_CALIBRATION_JSON_KEY_SLOPE_ERROR  "se"

enum HX711Gain : uint8_t
{
    A128 = 1,
    B64 = 3,
    B32 = 2,
};

struct HX711Calibration
{
    float offset, slope, offset_e, slope_e;
    bool set_offset = false, set_slope = false;

    bool to_json(JsonObject *obj);
    bool to_json(char *buf, size_t buf_len);
    bool from_json(JsonObject *obj);
    bool from_json(char *buf, size_t buf_len);
};

class HX711
{
private:
    const pin_size_t _pin_sck, _pin_dout;
    const HX711Gain _gain;

    inline void pulse();

public:
    HX711(pin_size_t pin_sck, pin_size_t pin_dout, HX711Gain gain=HX711Gain::A128);
    
    void begin();
    
    bool is_ready();
    void wait_ready();
    bool wait_ready_timeout(unsigned long timeout_ms=5000);
    
    bool read_raw_single(int32_t *raw, uint32_t timeout_ms=5000);
    bool read_raw_stats(uint32_t n, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms=5000);
    bool read_calib_stats(uint32_t n, HX711Calibration *calib, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms=5000);
    
    bool calib_offset(uint32_t n, HX711Calibration *calib, uint32_t *resulting_n, uint32_t timeout_ms=5000);
    bool calib_slope(uint32_t n, float weight, float weight_error, HX711Calibration *calib, uint32_t *resulting_n, uint32_t timeout_ms=5000);

    void power_off(bool wait_until_power_off=false);
    void power_on();
};

#endif /* _HX711_H_ */