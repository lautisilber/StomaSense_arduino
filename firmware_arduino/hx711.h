#include <sys/_stdint.h>
#ifndef _HX711_H_
#define _HX711_H_

#include <Arduino.h>
#include <ArduinoJson.h>

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

enum HXReturnCodes : int8_t
{
    OK = 0,

    HX_ERROR                                   = -1,
    HX_ERROR_TIMEOUT                           = -2,
    HX_ERROR_N_IS_0                            = -3,
    HX_ERROR_NOT_ENOUGH_READINGS_TO_DO_SATS    = -4,
    HX_ERROR_OFFSET_OR_SLOPE_NOT_SET           = -5,

    JSON_ERROR                                 = -10,
    JSON_ERROR_SAVING_OFFSET_FLAG_NOT_SET      = -11,
    JSON_ERROR_SAVING_BUFFER_LENGTH            = -12,
    JSON_ERROR_LOADING_NOT_JSONOBJECT          = -13,
    JSON_ERROR_LOADING_BAD_TYPING              = -14,
    JSON_ERROR_LOADING_SLOPE_DATA              = -15,
    JSON_ERROR_LOADING_BAD_DESERIALIZATION     = -16,
    JSON_ERROR_LOADING_BAD_SERIALIZATION       = -17,
    JSON_ERROR_LOADING_NO_SLOPE_DATA           = -18,
    JSON_ERROR_FILE_READ                       = -19,
    JSON_ERROR_FILE_WRITE                      = -20,

    ERROR_UNAVAILABLE_SLOT                     = -30,
    
    HX_WARNING                                 = 1,
    HX_WARNING_TIMEOUT                         = 2,
    HX_WARNING_DOING_STATS_WITH_N_1            = 3,

    JSON_WARNING                               = 10,
    JSON_WARNING_OFFSET_FLAG_NOT_SET           = 11,
    JSON_WARNING_SERIALIZED_LESS_THAN_MEASURED = 12,
};

struct HX711Calibration
{
    float offset, slope, offset_e, slope_e;
    bool set_offset = false, set_slope = false;

    HXReturnCodes to_json(JsonObject *obj);
    HXReturnCodes to_json(char *buf, size_t buf_len);
    HXReturnCodes from_json(JsonObject *obj);
    HXReturnCodes from_json(char *buf, size_t buf_len);
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
    HXReturnCodes wait_ready_timeout(unsigned long timeout_ms=5000);
    
    HXReturnCodes read_raw_single(int32_t *raw, uint32_t timeout_ms=5000);
    HXReturnCodes read_raw_stats(uint32_t n, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms=5000);
    HXReturnCodes read_calib_stats(uint32_t n, HX711Calibration *calib, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms=5000);
    
    HXReturnCodes calib_offset(uint32_t n, HX711Calibration *calib, uint32_t *resulting_n, uint32_t timeout_ms=5000);
    HXReturnCodes calib_slope(uint32_t n, float weight, float weight_error, HX711Calibration *calib, uint32_t *resulting_n, uint32_t timeout_ms=5000);

    void power_off(bool wait_until_power_off=false);
    void power_on();
};

#endif /* _HX711_H_ */