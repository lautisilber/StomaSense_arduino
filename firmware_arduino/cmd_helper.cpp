#include "cmd_helper.h"

static const char _cmd_success_prefix[] = "{\"success\"";
static const char _cmd_error_prefix[] = "{\"error\"";

static void _cmd_prefix_no_comma(Stream *stream, const char *cmd, const char *prefix)
{
    stream->print(prefix);
    stream->print(":true,\"cmd\":\"");
    stream->print(cmd);
    stream->print('"');
}
static inline void _cmd_prefix_comma(Stream *stream, const char *cmd, const char *prefix)
{
    _cmd_prefix_no_comma(stream, cmd, prefix);
    stream->print(',');
}

inline void CMD_Helper::cmd_success_prefix_no_comma(Stream *stream, const char *cmd)
{
    _cmd_prefix_no_comma(stream, cmd, _cmd_success_prefix);
}
inline void CMD_Helper::cmd_success_prefix_comma(Stream *stream, const char *cmd)
{
    _cmd_prefix_comma(stream, cmd, _cmd_success_prefix);
}
inline void CMD_Helper::cmd_error_prefix_no_comma(Stream *stream, const char *cmd)
{
    _cmd_prefix_no_comma(stream, cmd, _cmd_error_prefix);
}
inline void CMD_Helper::cmd_error_prefix_comma(Stream *stream, const char *cmd)
{
    _cmd_prefix_comma(stream, cmd, _cmd_error_prefix);
}

#define CMD_ERROR_PREFIX_NO_COMMA(stream, cmd) stream->printf(_cmd_error_format)
#define CMD_ERROR_PREFIX_WITH_COMMA(stream, cmd) do {stream->printf(_cmd_error_format); stream->print(',');} while(0)
#define CMD_POSTFIX(stream) stream->println('}')


static void _cmd_va_args(Stream *stream, const char *cmd, const char *fmt, va_list *args, void (*cmd_with_trail)(Stream *stream, const char *cmd, const char *trial));


void CMD_Helper::cmd_success(Stream *stream, const char *cmd)
{
    cmd_success_prefix_no_comma(stream, cmd);
    stream->println('}');
}

void CMD_Helper::cmd_success(Stream *stream, const char *cmd, JsonDocument *doc)
{
    (*doc)["success"] = true;
    (*doc)["cmd"] = cmd;
    serializeJson(*doc, *stream);
    stream->println();
}

void CMD_Helper::cmd_error(Stream *stream, const char *cmd, const char *msg)
{
    cmd_error_prefix_comma(stream, cmd);
    stream->print("\"msg\":\"");
    stream->print(msg);
    stream->println("\"}");
}

void CMD_Helper::cmd_success_with_trail(Stream *stream, const char *cmd, const char *trail)
{
    cmd_success_prefix_comma(stream, cmd);
    stream->print(trail);
    CMD_POSTFIX(stream);
}

void CMD_Helper::cmd_error_with_trail(Stream *stream, const char *cmd, const char *trail)
{
    cmd_error_prefix_comma(stream, cmd);
    stream->print(trail);
    CMD_POSTFIX(stream);
}

void CMD_Helper::cmd_success_va_args(Stream *stream, const char *cmd, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    _cmd_va_args(stream, cmd, fmt, &args, cmd_success_with_trail);
    va_end(args);
}

void CMD_Helper::cmd_error_va_args(Stream *stream, const char *cmd, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    _cmd_va_args(stream, cmd, fmt, &args, cmd_error_with_trail);
    va_end(args);
}

void CMD_Helper::cmd_received(Stream *stream, const char *cmd)
{
    stream->print("{\"cmd\":\"");
    stream->print(cmd);
    stream->println("\",\"processing\":true}");
}



static void _cmd_va_args(Stream *stream, const char *cmd, const char *fmt, va_list *args, void (*cmd_with_trail)(Stream *stream, const char *cmd, const char *trial))
{
    // I should call vprintf(fmt, args) for variadic stuff to work, but
    // Stream, that inherits from Print, doesn't implement it! So we can
    // use vsnprintf to copy it into a buffer and then printf

    va_list args_cpy;
    va_copy(args_cpy, *args);
    size_t s = vsnprintf(NULL, 0, fmt, args_cpy);
    va_end(args_cpy);

    char *buf = (char *)malloc(s + 1);

    if (buf)
    {
        vsnprintf(buf, s+1, fmt, *args);
        cmd_with_trail(stream, cmd, buf);
        free(buf);
    }
    else
    {
        CMD_Helper::cmd_error(stream, cmd, "Not enough available memory for response");
    }
}
