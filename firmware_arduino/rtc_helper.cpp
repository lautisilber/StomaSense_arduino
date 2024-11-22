#include "pico/types.h"
#include "rtc_helper.h"

// #include <cstddef>
// #include "pico/types.h"

#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "debug_helper.h"


namespace RTC {
    bool begin(datetime_t *dt)
    {
        if (!rtc_set_datetime(dt))
        {
            ERROR_PRINTLN("Couldn't set datetime for rtc");
            return false;
        }
        rtc_init();
        return true;
    }

    bool set_datetime(int16_t year, int8_t month, int8_t day, int8_t hour, int8_t minute, int8_t second)
    {
        datetime_t dt = {
            .year = year,
            .month = month,
            .day = day,
            .hour = hour,
            .min = minute,
            .sec = second
        };
        bool res = rtc_set_datetime(&dt);
        if (!res)
        {
            ERROR_PRINTLN("Error setting datetime for rtc");
        }
        return res;
    }

    bool set_datetime(const char *rtc_str)
    {
        int16_t year;
        int8_t month, day, hour, minute, second;
        const char format[] = "%i-%i-%i_%i-%i-%i";
        if (sscanf(rtc_str, format, &year, &month, &day, &hour, &minute, &second) != 6)
        {
            ERROR_PRINTFLN("Coudln't extract rtc from str '%s' because it didn't match format '%s'", rtc_str, format);
            return false;
        }
        return set_datetime(year, month, day, hour, minute, second);
    }

    bool get_datetime(datetime_t *dt)
    {
        if (!rtc_get_datetime(dt))
        {
            ERROR_PRINTLN("Couldn't get datetime from rtc. Maybe rtc is not running?");
            return false;
        }
        return true;
    }

    #define RTC_TIMESTAMP_STR_LENGTH 19
    static char _rtc_buf[RTC_TIMESTAMP_STR_LENGTH+1] = {0};
    const char *get_timestamp()
    {
        datetime_t dt;
        const char format[] = "%04d-%02d-%02d_%02d-%02d-%02d";
        if (!get_datetime(&dt)) return NULL;
        snprintf(_rtc_buf, RTC_TIMESTAMP_STR_LENGTH, format,
        dt.year%1000, dt.month%100, dt.day%100, dt.hour%100, dt.min%100, dt.sec%100);
        return (const char *)_rtc_buf;
    }
}