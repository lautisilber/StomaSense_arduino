#ifndef _CMD_HELPER_H_
#define _CMD_HELPER_H_

#include <Arduino.h>
#include <ArduinoJson.h>
#include <stdarg.h>

namespace CMD_Helper
{
    inline void cmd_success_prefix_no_comma(Stream *stream, const char *cmd);       // {"success": true, "cmd:": <cmd>
    inline void cmd_success_prefix_comma(Stream *stream, const char *cmd);          // {"success": true, "cmd:": <cmd>,
    inline void cmd_error_prefix_no_comma(Stream *stream, const char *cmd);         // {"error": true, "cmd:": <cmd>
    inline void cmd_error_prefix_comma(Stream *stream, const char *cmd);            // {"error": true, "cmd:": <cmd>,

    void cmd_success(Stream *stream, const char *cmd);                               // {"success": true, "cmd": <cmd>}
    void cmd_success(Stream *stream, const char *cmd, JsonDocument *doc);            // {"success": true, "cmd": <cmd>, **<doc>}
    void cmd_error(Stream *stream, const char *cmd, const char *msg);                // {"error": true, "cmd": <cmd>, "msg": <msg>}
    
    void cmd_success_with_trail(Stream *stream, const char *cmd, const char *trail); // {"success": true, "cmd": <cmd>, <trail>}
    void cmd_error_with_trail(Stream *stream, const char *cmd, const char *trail);   // {"error": true, "cmd": <cmd>, <trail>}
    
    void cmd_success_va_args(Stream *stream, const char *cmd, const char *fmt, ...); // printf("{"success": true, "cmd": <cmd>, fmt}", ...);
    void cmd_error_va_args(Stream *stream, const char *cmd, const char *fmt, ...);   // printf("{"error": true, "cmd": <cmd>, fmt}", ...);
    
    void cmd_received(Stream *stream, const char *cmd);                              // {"cmd": <cmd>, "processing": true}
}



#endif /* _CMD_HELPER_H_ */