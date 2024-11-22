#ifndef _RTC_HELPER_H_
#define _RTC_HELPER_H_


#include <Arduino.h>
#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "debug_helper.h"

namespace RTC
{
    inline void begin()
    {
        rtc_init();
    }
    bool begin(datetime_t *dt);

    bool set_datetime(const char *rtc_str);
    bool set_datetime(int16_t year, int8_t month, int8_t day, int8_t hour, int8_t minute, int8_t second);

    bool get_datetime(datetime_t *dt);

    const char *get_timestamp();
}

#endif /* _RTC_HELPER_H_ */