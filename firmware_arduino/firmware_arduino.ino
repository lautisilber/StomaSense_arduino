#include "defs.h"
#include "SmartComm.h"
#include "bme280_helper.h"
#include "stepper.h"
#include "hx711_mult.h"
#include "sd_helper.h"
#include "servo_helper.h"
#include "pump_helper.h"
#include "rtc_helper.h"
#include "run_data.h"
#include "Queues.h"
#include "eeprom_helper.h"
#include "SmartSortArray.h"
#include "PicoVector.h"
#include "cmd_helper.h"

/// Sensor handler classes ////////////////////////////////////////////////////////////////////////////////////////////////
BME280I2C bme(BME_Helper::default_settings);
HX711_Mult hx(HX711_MULT_1, HX711_MULT_2, HX711_MULT_3, HX711_MULT_4, HX711_SCK, HX711_DT);
ServoRPI servo(SERVO_PIN, true);
StepperAsync stepper(STEPPER_PIN_1, STEPPER_PIN_2, STEPPER_PIN_3, STEPPER_PIN_4, Stepper::StepType::HALF);
Pump pump(PUMP_PIN);

bool init_peripherals_flag = false;
void begin_peripherals()
{
    if (init_peripherals_flag) return;
    BME_Helper::begin(&bme, BME280_SDA_PIN, BME280_SCL_PIN);
    hx.begin();
    stepper.begin();
    RTC::begin();
    init_peripherals_flag = true;
}

/// Run data //////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define N_SCALES 6
RunData run_data;

/// Runtime ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool stomasense_setup(const char *rundata_json = NULL);
void stomasense_loop();
void water_loop();
volatile bool run_stomasense_loop = false;

/// Commands //////////////////////////////////////////////////////////////////////////////////////////////////////////////
// void cmd_error(Stream *stream, const char *cmd, const char *msg)
// {
//     stream->printf("{\"err\":true,\"cmd\":\"%s\",\"msg\":\"%s\"}\n", cmd, msg);
// }

void cmd_unknown(Stream *stream, const char *cmd)
{
    CMD_Helper::cmd_error_with_trail(stream, cmd, "\"unknown\":true");
}
SmartCmd cmd_ok("OK", [](Stream *stream, const SmartCmdArguments *args, const char *cmd) {
    CMD_Helper::cmd_success(stream, cmd);
});

SmartCmd cmd_bme("bme", [](Stream *stream, const SmartCmdArguments *args, const char *cmd) {
    // bme <literal_union[h, t, p]:data_to_retrieve>
    // example: bme ht -> returns humidity and temperature

    float hum, temp, pres;
    begin_peripherals();
    bool res = BME_Helper::read(&bme, &hum, &temp, &pres);
    const uint8_t err_msg_len = 29;
    const uint8_t min_size_for_new_entry = 11; // 11;
    const uint8_t buf_size = 3*min_size_for_new_entry + 3;
    static_assert(buf_size >= err_msg_len);
    // char buf[buf_size+1] = "{";
    char buf[buf_size+1] = {0};

    char *buf_last = buf;
    const char default_buf[] = "htp";
    const char *arg = default_buf;

    // this is a bit mask where the first bit is 1 only if 'h' has been processed,
    // the second is only 1 if 't' has been processed and the same for the third bit and 'p'
    uint8_t arg_mask = 0;
    const uint8_t arg_mask_h = 0b001, arg_mask_t = 0b010, arg_mask_p = 0b100;

    if (!res)
    {
        strncpy(buf, "Error reading bme", buf_size);
        goto error;
    }

    if (args->N > 0)
    {
        if (!args->to(0, &arg))
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
            if (arg_mask & arg_mask_h) continue;
            buf_last += snprintf(buf_last, buf_size-(buf-buf_last), "\"h\":%3.3f", fmod(hum,1000));
            arg_mask |= arg_mask_h;
            break;
        case 't':
            if (arg_mask & arg_mask_t) continue;
            buf_last += snprintf(buf_last, buf_size-(buf-buf_last), "\"t\":%3.3f", fmod(temp,1000));
            arg_mask |= 0b010;
            break;
        case 'p':
        if (arg_mask & arg_mask_p) continue;
            buf_last += snprintf(buf_last, buf_size-(buf-buf_last), "\"p\":%4.2f", fmod(pres,10000));
            arg_mask |= 0b100;
            break;
        case '\0':
            break;
        default:
            snprintf(buf, buf_size, "Invalid char in arg 0 '%c'", *(arg-1));
            goto error;
        }
        
        if (buf_last - buf >= buf_size || *arg == '\0')
        {
            // *(buf_last) = '}';
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
    // cmd_success_va_args(stream, cmd, "%s", buf);
    CMD_Helper::cmd_success_with_trail(stream, cmd, buf);
    return;

    // error
    error:
    CMD_Helper::cmd_error(stream, cmd, buf);
});

void hx_cb(Stream *stream, const SmartCmdArguments *args, const char *cmd, hx711_return_code_t(HX711_Mult::*hx_read)(uint8_t, uint32_t, float*, float*, uint32_t*, uint32_t), bool raw) {
    // hx | hx_raw <uint8_t:slot> <uint32_t:n_stat> <uint32_t:timeout_ms>

    if (args->N < 2)
    {
        CMD_Helper::cmd_error(stream, cmd, "Not enough arguments");
        return;
    }

    uint32_t timeout_ms = HX711_DEFAULT_TIMEOUT_MS;
    if (args->N >= 3)
    {
        if (!args->to(2, &timeout_ms))
        {
            CMD_Helper::cmd_error(stream, cmd, "Couldn't read timeout_ms (arg 2)");
            return;
        }
    }

    uint8_t slot;
    uint16_t n_stat;
    if (!args->to(0, &slot) || !args->to(1, &n_stat))
    {
        CMD_Helper::cmd_error(stream, cmd, "Not all arguments were ints");
        return;
    }

    if (n_stat > 1)
        CMD_Helper::cmd_received(stream, cmd);

    float mean;
    float stdev;
    uint32_t resulting_n;
    begin_peripherals();
    hx711_return_code_t res = ((hx).*(hx_read))(slot, n_stat, &mean, &stdev, &resulting_n, timeout_ms);

    if (HX711_IS_CODE_ERROR(res))
    {
        CMD_Helper::cmd_error_va_args(stream, cmd, "\"slot\":%u\"err_code\":%i", slot%1000, res);
        return;
    }

    CMD_Helper::cmd_success_va_args(stream, cmd, "\"mean\":%.4f,\"stdev\":%.4f,\"n\":%lu,\"slot\":%u,\"raw\":%s,\"code\":%i,\"code_txt\":\"%s\"", mean, stdev, resulting_n, slot, raw ? "true" : "false", res, hx711_mult_get_code_str(res));
}
SmartCmd cmd_hx("hx", [](Stream *stream, const SmartCmdArguments *args, const char *cmd) {
    hx_cb(stream, args, cmd, &HX711_Mult::read_calib_stats, false);
});
SmartCmd cmd_hx_raw("hx_raw", [](Stream *stream, const SmartCmdArguments *args, const char *cmd) {
    hx_cb(stream, args, cmd, &HX711_Mult::read_raw_stats, true);
});

SmartCmd cmd_rundata("rundata", [](Stream *stream, const SmartCmdArguments *args, const char *cmd) {
    // first argument is the literal "set", "save"
    // second argument is the json str to set (if first is "set")

    const char def_sub_cmd[] = "get";
    const char *sub_cmd = def_sub_cmd;
    if (args->N > 0)
    {
        if (!args->to(0, &sub_cmd))
        {
            CMD_Helper::cmd_error(stream, cmd, "Couldn't cast first argument to const char * (sub_cmd)");
            return;
        }

        if (strcmp(sub_cmd, "save") == 0)
        {
            if (!run_data.save())
            {
                CMD_Helper::cmd_error(stream, cmd, "Couldn't save run_data");
                return;
            }
        }
        else if (strcmp(sub_cmd, "set") == 0)
        {
            const char *json;
            if (!args->to(1, &json))
            {
                CMD_Helper::cmd_error(stream, cmd, "Couldn't cast first argument to const char * (json), or not enough arguments");
                return;
            }
            if (!run_data.set_data(json))
            {
                CMD_Helper::cmd_error_va_args(stream, cmd, "\"msg\":\"Couldn't set run_data\",\"json\":\"%s\"", json);
                // stream->printf("{\"err\":true,\"cmd\":\"rundata\",\"msg\":\"Couldn't set run_data with json '%s'\"}\n", json);
                return;
            }
        }
        else
        {
            CMD_Helper::cmd_error_va_args(stream, cmd, "\"msg\":\"Unknown subcommand '%s'\"", sub_cmd);
            // stream->printf("{\"err\":true,\"cmd\":\"rundata\",\"msg\":\"Unknown subcommand '%s'\"}\n", sub_cmd);
            return;
        }
    }

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    if (!run_data.get_data(&obj))
    {
        CMD_Helper::cmd_error(stream, cmd, "Couldn't get run_data because it is not populated");
        return;
    }
    obj["sub_cmd"] = sub_cmd;
    CMD_Helper::cmd_success(stream, cmd, &doc);
});

SmartCmd cmd_run("run", [](Stream *stream, const SmartCmdArguments *args, const char *cmd) {
    // the first argument has the following options
    // - if boolean true: it starts the mainloop with stored rundata
    // - if boolean false: it stops the mainloop if it was running
    // - if literal string "state", it returns a boolean in a json with the key "state" and the value indicating if the main loop is running
    if (args->N < 1)
    {
        CMD_Helper::cmd_error(stream, cmd, "Only one argument");
        return;
    }

    bool argument_processed = false;
    bool b;
    if (args->to(0, &b))
    {
        // is boolean
        if (!b)
        {
            // stop
            run_stomasense_loop = false;
            CMD_Helper::cmd_success_va_args(stream, cmd, "\"stopped\":true,\"state\":%s", run_stomasense_loop ? "true" : "false");
            // stream->printf("{\"cmd\":\"run\",\"stopped\":true,\"state\":%s}\n", run_stomasense_loop ? "true" : "false");
            return;
        }
        else
        {
            // run
            if (!stomasense_setup())
            {
                CMD_Helper::cmd_error(stream, cmd, "mainloop setup failed");
                return;
            }
            begin_peripherals();
            run_stomasense_loop = true;
            CMD_Helper::cmd_success_va_args(stream, cmd, "\"state\":%s", run_stomasense_loop ? "true" : "false");
            return;
        }
    }
    else
    {
        const char *s;
        if (!args->to(0, &s))
        {
            CMD_Helper::cmd_error(stream, cmd, "Couldn't cast first argument to boolean or const char *");
            return;
        }

        if (strcmp(s, "state") == 0)
        {
            // get state
            CMD_Helper::cmd_success_va_args(stream, cmd, "\"state\":%s", run_stomasense_loop ? "true" : "false");
            // stream->printf("{\"cmd\":\"run\",\"state\":%s}\n", run_stomasense_loop ? "true" : "false");
            return;
        }
        else
        {
            CMD_Helper::cmd_error_va_args(stream, cmd, "\"msg\":\"unknown sub_cmd '%s'\"", s);
            // stream->printf("{\"err\":true,\"cmd\":\"run\",\"msg\":\"unknown sub_cmd '%s'\"}\n", s);
            return;
        }
    }

    CMD_Helper::cmd_error(stream, cmd, "This shouldn't be reachable");
});

SmartCmd cmd_calib("hx_calib", [](Stream *stream, const SmartCmdArguments *args, const char *cmd) {
    // first argument should be one of the literals "offset", "slope", "save", "get", "set"
    //
    // if first argument is "set":
    //  second argument should be a json string containing the calibration
    //
    // if first argument is "offset", "slope":
    //  second argument is a uin8_t indicating slot to calibrate
    //  third argument is a uint32_t indicating the number of samples to take for the calibration (n)
    //
    //  if first argument was "slope":
    //      fourth and fifth arguments are weight and weight_error


    if (args->N < 1)
    {
        CMD_Helper::cmd_error(stream, cmd, "At least 1 argument needed");
        return;
    }

    const char *calib_stage;
    if (!args->to(0, &calib_stage))
    {
        CMD_Helper::cmd_error(stream, cmd, "Couldn't cast first argument to const char * (calib_stage)");
        return;
    }

    if (strcmp(calib_stage, "get") == 0)
    {
        goto end;
    }
    else if (strcmp(calib_stage, "save") == 0)
    {
        hx711_return_code_t code = hx.save_calibration();
        if (HX711_IS_CODE_ERROR(code))
        {
            CMD_Helper::cmd_error_va_args(stream, cmd, "\"msg\":\"Couldn't save calibration\",\"code\":%u\"code_txt\":\"%s\"", code, hx711_mult_get_code_str(code));
            return;
        }
        goto end;
    }
    else if (strcmp(calib_stage, "set") == 0)
    {
        if (args->N < 2)
        {
            CMD_Helper::cmd_error(stream, cmd, "At least 3 arguments needed for setting");
            return;
        }
        const char *json;
        if (!args->to(1, &json))
        {
            CMD_Helper::cmd_error(stream, cmd, "Couldn't cast second argument to const char * while setting (json)");
            return;
        }
        hx711_return_code_t code = hx.load_calibration(json);
        if (HX711_IS_CODE_ERROR(code))
        {
            CMD_Helper::cmd_error_va_args(stream, cmd, "\"msg\":\"Couldn't load calibration\",\"json\":\"%s\",\"code\":%u\"code_txt\":\"%s\"", json, code, hx711_mult_get_code_str(code));
            return;
        }
        goto end;
    }

    if (args->N < 3)
    {
        CMD_Helper::cmd_error(stream, cmd, "At least 3 arguments needed if calibrating");
        return;
    }

    uint8_t slot;
    if (!args->to(1, &slot))
    {
        CMD_Helper::cmd_error(stream, cmd, "Couldn't cast second argument to uint8_t (slot)");
        return;
    }
    if (slot >= N_MULTIPLEXERS)
    {
        CMD_Helper::cmd_error_va_args(stream, cmd, "\"msg\":\"Slot can't be %u when there are %u slots\"", slot, N_MULTIPLEXERS);
        // stream->printf("{\"err\":true,\"cmd\":\"hx_calib\",\"msg\":\"Slot can't be %u when there are %u slots\"}\n", slot, N_MULTIPLEXERS);
        return;
    }

    uint32_t n;
    if (!args->to(2, &n))
    {
        CMD_Helper::cmd_error(stream, cmd, "Couldn't cast fourth argument to uint32_t (n)");
        return;
    }

    uint32_t resulting_n;
    if (strcmp(calib_stage, "offset") == 0)
    {
        // bool calib_offset(uint8_t slot, uint32_t n, uint32_t *resulting_n, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);
        begin_peripherals();
        CMD_Helper::cmd_received(stream, cmd);
        hx711_return_code_t code = hx.calib_offset(slot, n, &resulting_n);
        if (HX711_IS_CODE_ERROR(code))
        {
            CMD_Helper::cmd_error_va_args(stream, cmd, "\"msg\":\"Error while calibrating offset\",\"code\":%u\"code_txt\":\"%s\"", code, hx711_mult_get_code_str(code));
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
            CMD_Helper::cmd_error(stream, cmd, "Couldn't cast first argument to float (weight)");
            return;
        }
        if (!args->to(5, &weight_error))
        {
            CMD_Helper::cmd_error(stream, cmd, "Couldn't cast fifth argument to float (weight_error)");
            return;
        }
        begin_peripherals();
        CMD_Helper::cmd_received(stream, cmd);
        hx711_return_code_t code = hx.calib_slope(slot, n, weight, weight_error, &resulting_n);
        if (HX711_IS_CODE_ERROR(code))
        {
            CMD_Helper::cmd_error_va_args(stream, cmd, "\"msg\":\"Error while calibrating slope\",\"code\":%u\"code_txt\":\"%s\"", code, hx711_mult_get_code_str(code));
            return;
        }
    }
    else
    {
        CMD_Helper::cmd_error(stream, cmd, "Bad first argument. Should have been 'offset', 'slope', 'save' or 'get'");
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
    obj["sub_cmd"] = calib_stage;
    CMD_Helper::cmd_success(stream, cmd, &doc);
});

SmartCmd cmd_rtc("rtc", [](Stream *stream, const SmartCmdArguments *args, const char *cmd) {
    // if first argument present, it is used to set the rtc. Should have format yyyy-mm-dd_HH-MM-SS

    const char *rtc_str;
    if (args->N > 0)
    {
        if (!args->to(0, &rtc_str))
        {
            CMD_Helper::cmd_error(stream, cmd, "Couldn't cast first argument to const char * (rtc_str)");
            return;
        }
        if (!RTC::set_datetime(rtc_str))
        {
            CMD_Helper::cmd_error_va_args(stream, cmd, "\"msg\":\"Coudln't set datetime for rtc with rtc_str '%s'\"", rtc_str);
            // stream->printf("{\"err\":true,\"cmd\":\"rtc\",\"msg\":\"Coudln't set datetime for rtc with rtc_str '%s'\"}\n", rtc_str);
            return;
        }
    }

    rtc_str = RTC::get_timestamp();
    CMD_Helper::cmd_success_va_args(stream, cmd, "\"rtc\",\"rtc_init\":%s,\"rtc_str\":\"%s\"", rtc_str ? "true" : "false", rtc_str);
    // stream->printf("{\"success\":true,\"cmd\":\"rtc\",\"rtc_init\":%s,\"rtc_str\":\"%s\"}\n", rtc_str ? "true" : "false", rtc_str);
});

SmartCmd cmd_pos("pos", [](Stream *stream, const SmartCmdArguments *args, const char *cmd) {
    // without arguments, it gets the current stepper pos and servo angle
    // with arguments, the first is the stepper position (optional), and the second is the servo angle (optional)

    bool stepper_pos_arg = false;
    int32_t stepper_pos;

    bool servo_angle_arg = false;
    uint8_t servo_angle;

    if (args->N > 0)
    {
        stepper_pos_arg = args->to(0, &stepper_pos);
        if (!stepper_pos_arg)
        {
            CMD_Helper::cmd_error(stream, cmd, "Couldn't cast first argument to int32_t (stepper_pos)");
            return;
        }
    }
    if (args->N > 1)
    {
        servo_angle_arg = args->to(1, &servo_angle);
        if (!servo_angle_arg)
        {
            CMD_Helper::cmd_error(stream, cmd, "Couldn't cast second argument to uint8_t (servo_angle)");
            return;
        }
    }

    begin_peripherals();
    CMD_Helper::cmd_received(stream, cmd);

    if (servo_angle_arg)
    {
        if (!servo.set_angle_slow_blocking(servo_angle))
        {
            CMD_Helper::cmd_error_va_args(stream, cmd, "\"msg\":\"Couldn't set servo to angle %u\"", servo_angle);
            // stream->printf("{\"err\":true,\"cmd\":\"pos\",\"msg\":\"Couldn't set servo to angle %u\"}\n", servo_angle);
            return;
        }
    }

    if (stepper_pos_arg)
    {
        // if (!)
        // {
        //     stream->printf("{\"err\":true,\"cmd\":\"pos\",\"msg\":\"Couldn't move stepper to pos %li\"}\n", stepper_pos);
        //     return;
        // }
        stepper.move_to_pos_blocking(stepper_pos, true);
    }

    CMD_Helper::cmd_success_va_args(stream, cmd, "\"stepper\":%li,\"servo\":%u", stepper.get_curr_pos(), servo.get_curr_angle());
    // stream->printf("{\"success\":true,\"cmd\":\"pos\",\"stepper\":%li,\"servo\":%u}\n", stepper.get_curr_pos(), servo.get_curr_angle());
});

SmartCmd cmd_stp_force("stp_force", [](Stream *stream, const SmartCmdArguments *args, const char *cmd) {
    // returns stepper pos at the end of the command
    // if a argument is provided (int32_t), the stepper pos will be updated to this new value, without actually moving
    // moving the stepper


    if (args->N != 1)
    {
        CMD_Helper::cmd_error(stream, cmd, "Number of arguments isn't 1. ");
    }

    int32_t new_pos;
    if (!args->to(0, &new_pos))
    {
        CMD_Helper::cmd_error(stream, cmd, "Couldn't cast first arg to int32_t (new_pos)");
        return;
    }
    // set_curr_pos_forced
    stepper.set_curr_pos_forced(new_pos);

    CMD_Helper::cmd_success_va_args(stream, cmd, "\"stepper\":%li", new_pos);
});

SmartCmd cmd_stp_flag("std_flag", [](Stream *stream, const SmartCmdArguments *args, const char *cmd) {
    // this command should be used to either read if the moving flag was left set, and eventually reset it
    // to reset, state first argument as boolean true

    bool save_ok = stepper.is_save_state_ok();
    bool reset_arg = false;
    bool reset;

    if (args->N > 0)
    {
        reset_arg = args->to(0, &reset);
        if (!reset_arg)
        {
            CMD_Helper::cmd_error(stream, cmd, "Couldn't cast argument 0 to bool (reset)");
            return;
        }
        if (reset)
            stepper.reset_save_state();
    }

    CMD_Helper::cmd_success_va_args(stream, cmd, "\"state_ok\":%s,\"state_reset\":",
        save_ok ? "true" : "false",
        reset_arg ? (reset ? "true" : "false") : "false"
    );
});

const SmartCmdBase *cmds[] = {
    &cmd_ok, &cmd_bme, &cmd_hx, &cmd_hx_raw, &cmd_run, &cmd_rundata, &cmd_calib, &cmd_rtc, &cmd_pos, &cmd_stp_force, &cmd_stp_flag
};

SmartComm<ARRAY_LENGTH(cmds)> sc(cmds, Serial, '\n', ' ', cmd_unknown);

void setup() {
    Serial.begin(115200);
    Serial.println("StomaSense v1.0.0");
}


void loop() {
    sc.tick();
    if (run_stomasense_loop)
    {
        stomasense_loop();
        water_loop();
    }
    delay(250);
}


/// logging ///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define N_READINGS_BEFORE_SAVING_TO_SD 50
constexpr uint32_t max_file_number = ULONG_MAX;
struct LogData
{
    const char *timestamp;
    uint8_t slot;
    float mean, stdev;
    uint32_t resulting_n;
    float hum, temp, pres;
    bool watered, finished_protocol;
    uint8_t protocol_step;
};

QueueFIFO<LogData, N_READINGS_BEFORE_SAVING_TO_SD> log_data_queue(false);

// template <typename T> T sgn(T val) {
//     return (T(0) < val) - (val < T(0));
// }
bool log_to_sd()
{
    static uint32_t curr_file_nr = 0;
    if (curr_file_nr >= max_file_number) return false;

    const size_t fname_len = 12;
    char fname[fname_len+1];
    snprintf(fname, fname_len, "%08x.TXT", curr_file_nr);
    fname[fname_len] = '\0';

    bool res = false;
    bool next_file = false;

    File file;
    if (!SD_Helper::open_write(&file, fname))
    {
        ERROR_PRINTFLN("Couldn't open file '%s' for logging", fname);
        goto end;
    }

    if (!file.println("time,slot,mean,stdev,n_readings,hum,temp,pres,watered,finished_protocol,protocol_step"))
    {
        ERROR_PRINTFLN("Couldn't write header on log flie '%s'", fname);
        goto end;
    }
    
    LogData ld;
    while (log_data_queue.available())
    {
        if (!log_data_queue.pop(&ld))
        {
            ERROR_PRINTLN("Error popping log data from queue");
            goto end;
        }


        if (!file.write(ld.timestamp))
        {
            ERROR_PRINTFLN("Couldn't write timestamp to file '%s'", fname);
            next_file = true;
            goto end;
        }

        const float max_float = 100000000000;
        const size_t buf_len = 77;
        char buf[buf_len+1];
        snprintf(buf, buf_len, ",%u,%f10.4,%f10.4,%lu,%3.3f,%3.3f,%4.2f,%u,%u,%u",
            ld.slot,
            fmod(ld.mean, max_float),
            fmod(ld.stdev, max_float),
            ld.resulting_n % 100000000,
            fmod(ld.hum, 1000),
            fmod(ld.temp, 1000),
            fmod(ld.pres, 10000),
            ld.watered,
            ld.finished_protocol,
            ld.protocol_step
        );
        if (!file.println(buf))
        {
            ERROR_PRINTFLN("Couldn't write to file '%s'", fname);
            next_file = true;
            goto end;
        }
    }

    next_file = true;
    res = true;

    end:
    if (!SD_Helper::close(&file))
    {
        ERROR_PRINTFLN("Couldn't close file '%s", fname);
        res = false;
    }
    if (next_file)
        ++curr_file_nr;
    return res;
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


#define WATERING_QUEUE_MAX_SIZE 10
PicoVector<ScalePosition, WATERING_QUEUE_MAX_SIZE> water_queue;


void stomasense_loop()
{
    if (!init_peripherals_flag)
    {
        ERROR_PRINTLN("Init peripherals flag not set");
        return;
    }
    if (water_queue.full())
    {
        INFO_PRINTLN("Waiting for water queue to empty");
        return;
    }

    static unsigned long bme_last_time = millis();
    static unsigned long hx_last_time = millis();
    static uint8_t curr_slot = 0;

    const ScalePosition *pos;
    ScaleProtocol *protocol;

    // wrap around
    if (curr_slot >= N_MULTIPLEXERS)
        curr_slot = 0;

    // check if current slot is in use
    if (!(run_data.get_scales_in_use()[curr_slot++]))
        return;

    // get data
    if (!run_data.get_slot_data(curr_slot, pos, protocol))
    {
        ERROR_PRINTFLN("Couldn't get data for slot in use %u. Doing nothing", curr_slot > 0 ? curr_slot - 1 : N_MULTIPLEXERS-1);
        return;
    }

    LogData ld = {.slot = curr_slot};

    // get weight
    float mean;
    // bool read_calib_stats(uint8_t slot, uint32_t n, float *mean, float *stdev, uint32_t *resulting_n, uint32_t timeout_ms=HX711_DEFAULT_TIMEOUT_MS);
    if (!hx.read_calib_stats(curr_slot, HX_STATS_N, &mean, &ld.stdev, &ld.resulting_n))
    {
        ERROR_PRINTFLN("Couldn't read calib stats for slot %u", curr_slot);
        return;
    }
    ld.mean = mean;

    // tick protocol
    // void tick(float weight, bool *should_water, bool *finished_protocol, uint8_t *curr_step=NULL);
    bool should_water;
    protocol->tick(mean, &should_water, &ld.finished_protocol, &ld.protocol_step);
    ld.watered = should_water;

    // water if necessary
    // TODO: make this async! (use queues)
    // if (should_water)
    // {
    //     servo.set_angle((SERVO_MAX_ANGLE - SERVO_MIN_ANGLE) / 2); // go to middle
    //     stepper.move_to_pos_blocking(pos->stepper);
    //     servo.set_angle_slow_blocking(pos->servo);
    //     pump.pump_blocking(pos->pump_intensity, pos->pump_time_us);
    //     servo.detach();
    // }
    if (should_water)
    {
        water_queue.push_back(pos);
    }

    log_data_queue.push(&ld);
    if (log_data_queue.full())
    {
        if (!log_to_sd())
        {
            ERROR_PRINTLN("Couldn't log entries to SD");
        }
    }
}

void water_loop()
{
    static bool new_position = true;
    static ScalePosition pos;
    if (!water_queue.available() || stepper.running())
    {
        return;
    }
    
    if (new_position)
    {
        // init movement
        water_queue.pop_front(&pos);

        servo.set_angle((SERVO_MAX_ANGLE - SERVO_MIN_ANGLE) / 2); // go to middle
        stepper.move_to_pos_async(pos.stepper);
        new_position = false;
    }
    else
    {
        // finish movement
        servo.set_angle_slow_blocking(pos.servo);
        pump.pump_blocking(pos.pump_intensity, pos.pump_time_us);
        servo.detach();
        new_position = true;
    }
}
