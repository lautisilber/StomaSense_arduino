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
#define HX711_CALIBRATION_JSON_KEY_SLOT         "r"
#define HX711_CALIBRATION_JSON_KEY_OFFSET       "o"
#define HX711_CALIBRATION_JSON_KEY_OFFSET_ERROR "p"
#define HX711_CALIBRATION_JSON_KEY_SLOPE        "s"
#define HX711_CALIBRATION_JSON_KEY_SLOPE_ERROR  "t"


#define HX711_OK                                  0

#define HX711_ERROR                               -1
#define HX711_ERROR_INVALID_SLOT                  -2
#define HX711_ERROR_CALIBRATION_NOT_SET           -3
#define HX711_ERROR_CALIBRATION_SAVE_FORMAT       -4
#define HX711_ERROR_CALIBRATION_DESERIALIZATION   -5
#define HX711_ERROR_CALIBRATION_SAVE_FILE         -6
#define HX711_ERROR_CALIBRATION_SERIALIZATION     -7
#define HX711_ERROR_CALIBRATION_BUFFER            -8
#define HX711_ERROR_TIMEOUT                       -9
#define HX711_ERROR_INSUFFICIENT_SAMPLES_FOR_STATS -10

#define HX711_WARN_CALIBRATION_OVERWRITTEN        1
#define HX711_WARN_CALIBRATION_NO_SLOPE_DATA      2
#define HX711_WARN_STATS_WITH_ONE_SAMPLE          3
#define HX711_WARN_CALIBRATION_NOT_LOADED         4

#define HX711_IS_CODE_OK(c)      (c == HX711_OK)
#define HX711_IS_CODE_ERROR(c)   (c < HX711_OK)
#define HX711_IS_CODE_WARNING(c) (c > HX711_OK)


typedef int8_t hx711_return_code_t;

enum HX711Gain : uint8_t
{
    A128 = 1,
    B64 = 3,
    B32 = 2,
};

struct HX711Calibration
{
    uint8_t slot;
    float offset, slope, offset_e, slope_e;
    bool set_offset = false, set_slope = false;

    hx711_return_code_t to_json(JsonObject *obj) const;
    hx711_return_code_t to_json(char *buf, size_t buf_len) const;
    hx711_return_code_t from_json(JsonObject *obj);
    hx711_return_code_t from_json(char *buf, size_t buf_len);
    bool populated() const;
};

class HX711
{
private:
    const pin_size_t _pin_sck, _pin_dout;
    const HX711Gain _gain;

public:
    HX711(pin_size_t pin_sck, pin_size_t pin_dout, HX711Gain gain=HX711Gain::A128);
    
    void begin();
    
    bool is_ready();
    void wait_ready();
    bool wait_ready_timeout(unsigned long timeout_ms=5000);
    
    hx711_return_code_t read_raw_single(int32_t *raw, uint32_t timeout_ms=5000);
    hx711_return_code_t read_raw_stats(uint32_t n, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms=5000);
    hx711_return_code_t read_calib_stats(uint32_t n, HX711Calibration *calib, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms=5000);
    
    hx711_return_code_t calib_offset(uint32_t n, HX711Calibration *calib, uint32_t *resulting_n, uint32_t timeout_ms=5000);
    hx711_return_code_t calib_slope(uint32_t n, float weight, float weight_error, HX711Calibration *calib, uint32_t *resulting_n, uint32_t timeout_ms=5000);

    void power_off(bool wait_until_power_off=false);
    void power_on();
};

extern const char *hx711_get_code_str(hx711_return_code_t code, bool *known=NULL);

#endif /* _HX711_H_ */