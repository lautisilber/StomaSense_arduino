#include "defs.h"
#include "SmartComm.h"
#include "bme280_helper.h"
#include "stepper.h"
#include "hx711_mult.h"
#include "sd_helper.h"
#include "servo_helper.h"
#include "pump_helper.h"
#include "rtc_helper.h"
#include "run.h"
#include "Queues.h"

/// Sensor handler classes ////////////////////////////////////////////////////////////////////////////////////////////////
BME280I2C bme;
HX711_Mult hx(HX711_MULT_1, HX711_MULT_2, HX711_MULT_3, HX711_MULT_4, HX711_SCK, HX711_DT);
ServoRPI servo(SERVO_PIN, SERVO_MIN_DUTY, SERVO_MAX_DUTY);

/// Run data //////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define N_SCALES 6
RunData<N_SCALES> run_data;

/// Runtime ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool stomasense_setup(const char *rundata_json = NULL);
void stomasense_loop();
volatile bool run_stomasense_loop = false;

/// Commands //////////////////////////////////////////////////////////////////////////////////////////////////////////////
void cmd_error(Stream *stream, const char *cmd, const char *msg)
{
    stream->printf("{\"err\":true,\"cmd\":\"%s\",\"msg\":\"%s\"}\n", cmd, msg);
}
void cmd_received(Stream *stream, const char *cmd)
{
    stream->printf("{\"cmd\":\"%s\",\"processing\":true}\n", cmd);
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

void hx_cb(Stream *stream, const SmartCmdArguments *args, const char *cmd, bool(HX711_Mult::*hx_read)(uint8_t, uint32_t, float*, float*, uint32_t*, uint32_t), bool raw) {
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

SmartCmd cmd_rundata("rundata", [](Stream *stream, const SmartCmdArguments *args, const char *cmd){
    // first argument is the literal "set", "save"
    // second argument is the json str to set (if first is "set")

    const char def_sub_cmd[] = "get";
    const char *sub_cmd = def_sub_cmd;
    if (args->N > 0)
    {
        if (!args->to(0, sub_cmd))
        {
            cmd_error(stream, cmd, "Couldn't cast first argument to const char * (sub_cmd)");
            return;
        }

        if (strcmp(sub_cmd, "save") == 0)
        {
            if (!run_data.save())
            {
                stream->println("{\"err\":true,\"cmd\":\"rundata\",\"msg\":\"Couldn't save run_data\"}");
                return;
            }
        }
        else if (strcmp(sub_cmd, "set") == 0)
        {
            const char *json;
            if (!args->to(1, json))
            {
                cmd_error(stream, cmd, "Couldn't cast first argument to const char * (json), or not enough arguments");
                return;
            }
            if (!run_data.set_data(json))
            {
                stream->printf("{\"err\":true,\"cmd\":\"rundata\",\"msg\":\"Couldn't set run_data with json '%s'\"}\n", json);
                return;
            }
        }
        else
        {
            stream->printf("{\"err\":true,\"cmd\":\"rundata\",\"msg\":\"Unknown subcommand '%s'\"}\n", sub_cmd);
            return;
        }
    }

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    if (!run_data.get_data(&obj))
    {
        cmd_error(stream, cmd, "Couldn't get run_data because it is not populated");
        return;
    }
    obj["success"] = true;
    obj["cmd"] = cmd;
    obj["sub_cmd"] = sub_cmd;
    serializeJson(obj, *stream);
    stream->print("\n");
});

SmartCmd cmd_run("run", [](Stream *stream, const SmartCmdArguments *args, const char *cmd){
    // the first argument has the following options
    // - if boolean true: it starts the mainloop with stored rundata
    // - if boolean false: it stops the mainloop if it was running
    // - if literal string "state", it returns a boolean in a json with the key "state" and the value indicating if the main loop is running
    if (args->N < 1)
    {
        cmd_error(stream, cmd, "Only one argument");
        return;
    }

    bool argument_processed = false;
    bool b;
    if (args->to(0, &b))
    {
        // is boolean
        if (!b)
        {
            run_stomasense_loop = false;
            stream->printf("{\"cmd\":\"run\",\"stopped\":true,\"state\":%s}\n", run_stomasense_loop ? "true" : "false");
            return;
        }
        else
        {
            if (!stomasense_setup())
            {
                cmd_error(stream, cmd, "mainloop setup failed");
                return;
            }
        }
    }
    else
    {
        const char *s;
        if (!args->to(0, s))
        {
            cmd_error(stream, cmd, "Couldn't cast first argument to boolean or const char *");
            return;
        }

        if (strcmp(s, "state") == 0)
        {
            stream->printf("{\"cmd\":\"run\",\"state\":%s}\n", run_stomasense_loop ? "true" : "false");
            return;
        }
        else
        {
            stream->printf("{\"err\":true,\"cmd\":\"run\",\"msg\":\"unknown sub_cmd '%s'\"}\n", s);
            return;
        }
    }

    cmd_error(stream, cmd, "This shouldn't be reachable");
});

SmartCmd cmd_calib("calib", [](Stream *stream, const SmartCmdArguments *args, const char *cmd){
    // first argument should be one of the literals "offset", "slope", "save", "get", "set"
    // second argument is a uin8_t indicating slot to calibrate. if the first was set, the second command is a json array with the calibartion data
    // third argument must be a boolean stating whether results should be stored to SD if successful. this works the same for "set"
    // fourth argument is a uint32_t indicating the number of samples to take for the calibration (n)
    // fifth and sixth arguments are weight and weight_error and will be used only if first argument is "slope"


    const size_t len_sub_cmd = 6;
    bool save_to_sd = false;

    if (args->N < 1)
    {
        cmd_error(stream, cmd, "At least 1 argument needed");
        return;
    }

    const char *calib_stage;
    if (!args->to(0, calib_stage))
    {
        cmd_error(stream, cmd, "Couldn't cast first argument to const char * (calib_stage)");
        return;
    }

    if (strcmp(calib_stage, "get") == 0)
    {
        goto end;
    }
    else if (strcmp(calib_stage, "save") == 0)
    {
        save_to_sd = true;
        goto end;
    }
    else if (strcmp(calib_stage, "set") == 0)
    {
        if (args->N < 3)
        {
            cmd_error(stream, cmd, "At least 3 arguments needed for setting");
            return;
        }
        const char *json;
        if (!args->to(1, json))
        {
            cmd_error(stream, cmd, "Couldn't cast second argument to const char * while setting (json)");
            return;
        }
        if (!args->to(2, &save_to_sd))
        {
            cmd_error(stream, cmd, "Couldn't cast third argument to bool while setting (save_to_sd)");
            return;
        }
        if (!hx.load_calibration(json))
        {
            stream->printf("{\"err\":true,\"cmd\":\"calib\",\"msg\":\"Couldn't load calibration with json '%s'\"}\n", json);
            return;
        }
        goto end;
    }

    if (args->N < 4)
    {
        cmd_error(stream, cmd, "At least 4 arguments needed if not saving, setting or getting");
        return;
    }

    uint8_t slot;
    if (!args->to(1, &slot))
    {
        cmd_error(stream, cmd, "Couldn't cast second argument to uint8_t (slot)");
        return;
    }
    if (slot >= N_MULTIPLEXERS)
    {
        stream->printf("{\"err\":true,\"cmd\":\"calib\",\"msg\":\"Slot can't be %u when there are %u slots\"}\n", slot, N_MULTIPLEXERS);
        return;
    }

    if (!args->to(2, &save_to_sd))
    {
        cmd_error(stream, cmd, "Couldn't cast third argument to bool (save_to_sd)");
        return;
    }

    uint32_t n;
    if (!args->to(3, &n))
    {
        cmd_error(stream, cmd, "Couldn't cast fourth argument to uint32_t (n)");
        return;
    }

    uint32_t resulting_n;
    if (strcmp(calib_stage, "offset") == 0)
    {
        // bool calib_offset(uint8_t slot, uint32_t n, uint32_t *resulting_n, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);
        cmd_received(stream, cmd);
        if (!hx.calib_offset(slot, n, &resulting_n))
        {
            cmd_error(stream, cmd, "Error while calibrating offset");
            return;
        }
        goto end;
    }
    else if (strcmp(calib_stage, "slope") == 0)
    {
        // bool calib_slope(uint8_t slot, uint32_t n, float weight, float weight_error, uint32_t *resulting_n, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);
        float weight, weight_error;
        if (!args->to(4, &weight))
        {
            cmd_error(stream, cmd, "Couldn't cast first argument to float (weight)");
            return;
        }
        if (!args->to(5, &weight_error))
        {
            cmd_error(stream, cmd, "Couldn't cast fifth argument to float (weight_error)");
            return;
        }
        cmd_received(stream, cmd);
        if (!hx.calib_slope(slot, n, weight, weight_error, &resulting_n))
        {
            cmd_error(stream, cmd, "Error while calibrating slope");
            return;
        }
    }
    else
    {
        cmd_error(stream, cmd, "Bad first argument. Should have been 'offset', 'slope', 'save' or 'get'");
        return;
    }

    end:

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    JsonArray calibs = obj["calibs"].to<JsonArray>();
    const HX711Calibration *calib_ptr = hx.get_calibs();
    for (size_t i = 0; i < N_MULTIPLEXERS; i++)
    {
        if (!calib_ptr[i].populated()) continue;
        JsonObject calib_obj = calibs.add<JsonObject>();
        if (!calib_ptr[i].to_json(&calib_obj))
        {
            WARN_PRINTFLN("Couldn't retrieve the json from calibration from slot %u", i);
        }
    }
    obj["cmd"] = cmd;
    obj["sub_cmd"] = calib_stage;
    if (!save_to_sd)
    {
        obj["success"] = true;
        serializeJson(obj, *stream);
        return;
    }

    if (hx.save_calibration())
    {
        obj["success"] = true;
        obj["msg"] = "saved calibration";
    }
    else
    {
        obj["error"] = true;
        obj["msg"] = "couldn't save calibration";
    }
    serializeJson(obj, *stream);
    stream->print("\n");
});

SmartCmd cmd_rtc("rtc", [](Stream *stream, const SmartCmdArguments *args, const char *cmd){
    // if first command present, it is used to set the rtc. Should have format yyyy-mm-dd_HH-MM-SS

    const char *rtc_str;
    if (args->N > 0)
    {
        if (!args->to(0, rtc_str))
        {
            cmd_error(stream, cmd, "Couldn't cast first argument to const char * (rtc_str)");
            return;
        }
        if (!RTC::set_datetime(rtc_str))
        {
            stream->printf("{\"err\":true,\"cmd\":\"rtc\",\"msg\":\"Coudln't set datetime for rtc with rtc_str '%s'\"}\n", rtc_str);
            return;
        }
    }

    rtc_str = RTC::get_timestamp();
    stream->printf("{\"success\":true,\"cmd\":\"rtc\",\"rtc_init\":%s,\"rtc_str\":\"%s\"}\n", rtc_str ? "true" : "false", rtc_str);
});

const SmartCmdBase *cmds[] = {
    &cmd_ok, &cmd_bme, &cmd_hx, &cmd_hx_raw, &cmd_run, &cmd_rundata, &cmd_calib, &cmd_rtc
};

SmartComm<ARRAY_LENGTH(cmds)> sc(cmds, Serial);

void setup() {
    Serial.begin(115200);
    Serial.println("StomaSense v1.0.0");

    BME_HELPER::begin(&bme, BME280_SDA_PIN, BME280_SCL_PIN);

    hx.begin();
    RTC::begin();
}


void loop() {
    sc.tick();
    if (run_stomasense_loop)
        stomasense_loop();
    delay(250);
}


bool stomasense_setup(const char *rundata_json)
{
    if (rundata_json)
    {
        if (!run_data.set_data(rundata_json))
        {
            ERROR_PRINTFLN("Coudln't set data for run_data with json '%s'", rundata_json);
            return false;
        }
    }
    return true;
}

void stomasense_loop()
{
    static unsigned long bme_last_time = millis();
    static unsigned long hx_last_time = millis();
    static uint8_t curr_scale = 0;

    
}
