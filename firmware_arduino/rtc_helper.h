#ifndef _RTC_HELPER_H_
#define _RTC_HELPER_H_

#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "debug_helper.h"
#include <ArduinoJson.h>

namespace RTC
{
    inline void begin()
    {
        rtc_init();
    }
    bool begin(datetime_t *dt);

    #define RTC_JSON_KEY_YEAR   "y"
    #define RTC_JSON_KEY_MONTH  "m"
    #define RTC_JSON_KEY_DAY    "d"
    #define RTC_JSON_KEY_HOUR   "H"
    #define RTC_JSON_KEY_MINUTE "M"
    #define RTC_JSON_KEY_SECOND "S"

    bool set_datetime(JsonDocument *doc);
    bool set_datetime(const char *json);

    bool get_datetime(datetime_t *dt);

    const char *get_timestamp();
}

#endif /* _RTC_HELPER_H_ */