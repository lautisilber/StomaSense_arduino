#ifndef _HX711_MULT_H_
#define _HX711_MULT_H_

#include <Arduino.h>
#include <ArduinoJson.h>
#include "hx711.h"
#include "defs.h"

#ifndef HX711_DEFAULT_TIMEOUT_MS
#define HX711_DEFAULT_TIMEOUT_MS 5000
#endif

#define HX711_SAVEFILE "HXCALIB.JSN"

#if N_MULTIPLEXERS <= 0
#error "There should be at least 1 scale"
#endif

class HX711_Mult
{
private:
    HX711 _hx;
    const pin_size_t _mult_pin_1, _mult_pin_2, _mult_pin_3, _mult_pin_4;
    int8_t _curr_slot = -1;
    HX711Calibration _calibs[N_MULTIPLEXERS];
    bool _set_calibs[N_MULTIPLEXERS];

    void _set_slot(uint8_t slot);

public:
    HX711_Mult(pin_size_t mult_pin_1, pin_size_t mult_pin_2, pin_size_t mult_pin_3, pin_size_t mult_pin_4,
        pin_size_t pin_sck, pin_size_t pin_dout, HX711Gain gain=HX711Gain::A128);

    void begin();

    void set_slot(uint8_t slot);

    hx711_return_code_t read_raw_single(uint8_t slot, int32_t *raw, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);
    hx711_return_code_t read_raw_stats(uint8_t slot, uint32_t n, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);
    hx711_return_code_t read_calib_stats(uint8_t slot, uint32_t n, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);

    hx711_return_code_t calib_offset(uint8_t slot, uint32_t n, uint32_t *resulting_n, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);
    hx711_return_code_t calib_slope(uint8_t slot, uint32_t n, float weight, float weight_error, uint32_t *resulting_n, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);

    hx711_return_code_t power_down(uint8_t slot, bool wait_until_power_off=false);
    hx711_return_code_t power_up(uint8_t slot);

    hx711_return_code_t load_calibration();
    hx711_return_code_t load_calibration(const char *json);
    hx711_return_code_t load_calibration(JsonDocument *doc);
    hx711_return_code_t save_calibration();

    inline const HX711Calibration *get_calibs() const { return (const HX711Calibration *)_calibs; }
    inline const bool *get_set_calibs() const { return (const bool *)_set_calibs; }
};

extern const char *hx711_mult_get_code_str(hx711_return_code_t code, bool *known=NULL);

#endif /* _HX711_MULT_H_ */

