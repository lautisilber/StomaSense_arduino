#ifndef _RUN_H_
#define _RUN_H_

#include <Arduino.h>
#include <ArduinoJson.h>

#include "scale_protocol.h"
#include "scale_position.h"

#define RUN_JSON_KEY_SCALE_POSITIONS                "p"
#define RUN_JSON_KEY_SCALE_PROTOCOLS                "q"
#define RUN_JSON_KEY_BME_TIME_MS                    "b"

#define RUN_SAVE_FILE "RUNDATA.JSN"

// template <size_t N_SCALES>
class RunData
{
private:
    ScalePosition _scale_positions[N_MULTIPLEXERS];
    ScaleProtocol _scale_protocols[N_MULTIPLEXERS];
    bool _scales_in_use[N_MULTIPLEXERS];
    uint32_t _bme_time_ms;
    bool _init_flag = false;

public:
    const ScalePosition *get_slot_position(uint8_t slot) const;
    ScaleProtocol *get_slot_protocol(uint8_t slot);
    bool get_slot_data(uint8_t slot, const ScalePosition *pos, ScaleProtocol *protocol);

    inline const bool *get_scales_in_use() const { return (const bool *)_scales_in_use; }

    bool set_data(JsonDocument *doc);
    bool set_data(const char *json_str);
    bool set_data(Stream *stream);

    bool get_data(JsonObject *obj) const;

    bool save() const;
    bool load();
};



#endif /* _RUN_H_ */