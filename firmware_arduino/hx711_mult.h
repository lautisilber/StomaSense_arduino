#ifndef _HX711_MULT_H_
#define _HX711_MULT_H_

#include <Arduino.h>
#include "hx711.h"

#ifndef HX711_DEFAULT_TIMEOUT_MS
#define HX711_DEFAULT_TIMEOUT_MS 5000
#endif

#define pwrtwo(x) (1 << (x))
#define N_MULTIPLEXER_PINS 4
#define N_MULTIPLEXERS pwrtwo(N_MULTIPLEXER_PINS)
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

    void _set_slot(uint8_t slot);

    static void print_line(uint32_t line)
    #if defined(D_INFO)
    {
        INFO_PRINTFLN("HX code generated in HX_Mult from line %ul", line);
    }
    #else
    {}
    #endif

public:
    HX711_Mult(pin_size_t mult_pin_1, pin_size_t mult_pin_2, pin_size_t mult_pin_3, pin_size_t mult_pin_4,
        pin_size_t pin_sck, pin_size_t pin_dout, HX711Gain gain=HX711Gain::A128);

    void begin();

    void set_slot(uint8_t slot);

    bool read_raw_single(uint8_t slot, int32_t *raw, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);
    bool read_raw_stats(uint8_t slot, uint32_t n, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);
    bool read_calib_stats(uint8_t slot, uint32_t n, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);

    bool calib_offset(uint8_t slot, uint32_t n, uint32_t *resulting_n, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);
    bool calib_slope(uint8_t slot, uint32_t n, float weight, float weight_error, uint32_t *resulting_n, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);

    bool power_down(uint8_t slot, bool wait_until_power_off=false);
    bool power_up(uint8_t slot);

    bool load_calibration();
    bool save_calibration();
};

#endif /* _HX711_MULT_H_ */

