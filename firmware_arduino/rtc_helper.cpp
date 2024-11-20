#include "rtc_helper.h"

// #include <cstddef>
// #include "pico/types.h"

#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "debug_helper.h"
#include <ArduinoJson.h>


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

    bool set_datetime(JsonDocument *doc)
    {
        if (
            !(*doc)[RTC_JSON_KEY_YEAR].is<uint16_t>() ||
            !(*doc)[RTC_JSON_KEY_MONTH].is<uint8_t>() ||
            !(*doc)[RTC_JSON_KEY_DAY].is<uint8_t>() ||
            !(*doc)[RTC_JSON_KEY_HOUR].is<uint8_t>() ||
            !(*doc)[RTC_JSON_KEY_MINUTE].is<uint8_t>() ||
            !(*doc)[RTC_JSON_KEY_SECOND].is<uint8_t>()
        )
        {
            ERROR_PRINTLN("Couldn't get datetime from JsonDocument: Keys messing or badly typed");
            return false;
        }

        datetime_t dt;
        dt.year = (*doc)[RTC_JSON_KEY_YEAR].as<uint16_t>();
        dt.month = (*doc)[RTC_JSON_KEY_MONTH].as<uint8_t>();
        dt.day = (*doc)[RTC_JSON_KEY_DAY].as<uint8_t>();
        dt.hour = (*doc)[RTC_JSON_KEY_HOUR].as<uint8_t>();
        dt.min = (*doc)[RTC_JSON_KEY_MINUTE].as<uint8_t>();
        dt.sec = (*doc)[RTC_JSON_KEY_SECOND].as<uint8_t>();

        return rtc_set_datetime(&dt);
    }

    bool set_datetime(const char *json)
    {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, json);
        if (error)
        {
            ERROR_PRINTFLN("Couldn't deserialize json to set datetime.\n\tOriginal str: '%s'\n\tError: '%s'\n", json, error.c_str());
            return false;
        }
        return set_datetime(&doc);
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
        if (!get_datetime(&dt)) return NULL;
        snprintf(_rtc_buf, RTC_TIMESTAMP_STR_LENGTH, "%04d-%02d-%02d %02d:%02d:%02d",
        dt.year%1000, dt.month%100, dt.day%100, dt.hour%100, dt.min%100, dt.sec%100);
        return (const char *)_rtc_buf;
    }
}