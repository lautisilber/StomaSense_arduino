#include "defs.h"
#include "SmartComm.h"
#include "bme280_helper.h"
#include "stepper.h"
#include "hx711_mult.h"
#include "sd_helper.h"
#include "servo_helper.h"
#include "pump_helper.h"
#include "rtc_helper.h"
#include "FIFOStack.h"


/// Sensor handler classes ////////////////////////////////////////////////////////////////////////////////////////////////
BME280I2C bme;
HX711_Mult hx(HX711_MULT_1, HX711_MULT_2, HX711_MULT_3, HX711_MULT_4, HX711_SCK, HX711_DT);
ServoRPI servo(SERVO_PIN, SERVO_MIN_DUTY, SERVO_MAX_DUTY);

/// Commands //////////////////////////////////////////////////////////////////////////////////////////////////////////////
void cmd_error(Stream *stream, const char *cmd, const char *msg)
{
    stream->printf("{\"err\":true,\"cmd\":\"%s\",\"msg\":\%s\"}\n", cmd, msg);
}

// SMART_CMD_CREATE_RAM(cmdOk, "OK", [](Stream *stream, const SmartCmdArguments *args, const char *cmd){
//     stream->println("OK");
// });
SmartCmd cmd_ok("OK", [](Stream *stream, const SmartCmdArguments *args, const char *cmd) {
    stream->println("OK");
});

SmartCmd cmd_bme("bme", [](Stream *stream, const SmartCmdArguments *args, const char *cmd) {
    // bme <literal_union[h, t, p]:data_to_retrieve>
    // example: bme ht -> returns humidity and temperature

    float hum, temp, pres;
    bool res = BME_HELPER::read(&bme, &hum, &temp, &pres);
    const uint8_t err_msg_len = 29;
    const uint8_t min_size_for_new_entry = 11;
    const uint8_t buf_size = 3*min_size_for_new_entry + 3;
    static_assert(buf_size >= err_msg_len);
    char buf[buf_size+1] = "{";

    char *buf_last = buf+1;
    const char default_buf[] = "htp";
    const char *arg;

    if (!res)
    {
        strncpy(buf, "Error reading bme", buf_size);
        goto error;
    }

    if (args->N == 0)
        arg = default_buf;
    else
    {
        buf[0] = '{';
        if (!args->to(0, arg))
        {
            strncpy(buf, "Error getting arg 0", buf_size);
            goto error;
        }
    }

    for (;;)
    {
        switch (*(arg++))
        {
        case 'h':
            buf_last += snprintf(buf_last, buf_size-(buf-buf_last), "\"h\":%3.3f", fmod(hum,1000));
            break;
        case 't':
            buf_last += snprintf(buf_last, buf_size-(buf-buf_last), "\"t\":%3.3f", fmod(temp,1000));
            break;
        case 'p':
            buf_last += snprintf(buf_last, buf_size-(buf-buf_last), "\"p\":%4.2f", fmod(pres,10000));
            break;
        default:
            snprintf(buf, buf_size, "Invalid char in arg 0 '%c'", *(arg-1));
            goto error;
        }
        
        if (buf_last - buf >= buf_size || *arg == '\0')
        {
            *(buf_last) = '}';
            break;
        }
        else if (buf_last - buf < buf_size-min_size_for_new_entry)
        {
            *(buf_last++) = ',';
        }
        else
        {
            snprintf(buf, buf_size, "Error. Not enough space (%i)", (buf_last - buf < 1000 ? buf_last - buf : -1));
            goto error;
        }
    }
    stream->println(buf);
    return;

    // error
    error:
    cmd_error(stream, cmd, buf);
});

void hx_cb(Stream *stream, const SmartCmdArguments *args, const char *cmd, HXReturnCodes(HX711_Mult::*hx_read)(uint8_t, uint32_t, float*, float*, uint32_t*, uint32_t), bool raw) {
    // hx | hx_raw <uint8_t:slot> <uint32_t:n_stat> <uint32_t:timeout_ms>

    if (args->N < 2)
    {
        cmd_error(stream, cmd, "Not enough arguments");
        return;
    }

    uint32_t timeout_ms = HX711_DEFAULT_TIMEOUT_MS;
    if (args->N >= 3)
    {
        if (!args->to(2, &timeout_ms))
        {
            cmd_error(stream, cmd, "Couldn't read timeout_ms (arg 2)");
            return;
        }
    }

    uint8_t slot;
    uint16_t n_stat;
    if (!args->to(0, &slot) || !args->to(1, &n_stat))
    {
        cmd_error(stream, cmd, "Not all arguments were ints");
        return;
    }

    stream->println("rcv");

    float mean;
    float stdev;
    uint32_t resulting_n;
    bool res = ((hx).*(hx_read))(slot, n_stat, &mean, &stdev, &resulting_n, timeout_ms);

    if (!res)
    {
        const uint8_t buf_len = 33;
        char buf[buf_len] = "Error reading hx raw in slot ";
        snprintf(buf, buf_len-1, "%u", slot%1000);
        cmd_error(stream, cmd, buf);
    }

    stream->printf("{\"mean\":%.4f,\"stdev\":%.4f,\"n\":%ul,\"slot\":%u,\"raw\":%s}\n", mean, stdev, resulting_n, slot, raw ? "true" : "false");
}
SmartCmd cmd_hx("hx", [](Stream *stream, const SmartCmdArguments *args, const char *cmd){
    hx_cb(stream, args, cmd, &HX711_Mult::read_calib_stats, false);
});
SmartCmd cmd_hx_raw("hx_raw", [](Stream *stream, const SmartCmdArguments *args, const char *cmd){
    hx_cb(stream, args, cmd, &HX711_Mult::read_raw_stats, true);
});

const SmartCmdBase *cmds[] = {
    &cmd_ok, &cmd_bme, &cmd_hx_raw
};

SmartComm<ARRAY_LENGTH(cmds)> sc(cmds, Serial);



/// hx multicore //////////////////////////////////////////////////////////////////////////////////////////////////////////
#define HX_REQUESTS_STACK_SIZE 32
#define HX_DATA_STACK_SIZE     128

struct HXReadingRequest
{
    uint8_t slot;
    uint32_t n;
};
struct HXReadingData
{
    bool success;
    uint8_t slot;
    float mean, stdev;
    uint32_t n_readings;
};
// stack to request hx readings
FIFOStackThreadSafeRP2040<HXReadingRequest, HX_REQUESTS_STACK_SIZE> hx_reading_requests;
// stack to store hx readings
FIFOStackThreadSafeRP2040<HXReadingData, HX_DATA_STACK_SIZE> hx_reading_data;
bool multicore_request_hx_reading(uint8_t slot, uint32_t n)
{
    // https://arduino-pico.readthedocs.io/en/latest/multicore.html
    return false;
}

void loop1()
{
    while (hx_reading_requests.available_blocking())
    {
        HXReadingRequest req;
        HXReadingData res;

        if (!hx_reading_requests.pop_blocking(req));
        res.slot = req.slot;
        res.success = hx.read_calib_stats(req.slot, req.n, &res.mean, &res.stdev, &res.n_readings);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("StomaSense v1.0.0");

    BME_HELPER::begin(&bme, BME280_SDA_PIN, BME280_SCL_PIN);

    rp2040.idleOtherCore();
    hx.begin();
}


void loop() {
    // float hum, temp, pres;
    // if (!BME_HELPER::read(&bme, &hum, &temp, &pres))
    //     Serial.println("bme -> ERROR!");
    // else
    //     Serial.printf("bme -> t: %f.2 *C, h: %f.2 %, p: %f.2 hPa\n", temp, hum, pres);

    // delay(2000);

    sc.tick();
    delay(250);
}
