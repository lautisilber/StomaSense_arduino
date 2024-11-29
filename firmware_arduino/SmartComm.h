#ifndef _SIMPLE_COMM_H_
#define _SIMPLE_COMM_H_

#include <Arduino.h>
#include <limits.h>

#define ARRAY_LENGTH(a) sizeof(a)/sizeof(a[0])

#ifndef MAX_ARGUMENTS
#define MAX_ARGUMENTS 16 // lim of args is actually MAX_ARGUMENTS-1 since the first argument is actually the command
#else
    #if defined(ARDUINO_ARCH_AVR) && MAX_ARGUMENTS > UCHAR_MAX
    #error "Cannot define MAX_ARGUMENTS to be greater than " #UCHAR_MAX " and it was " #MAX_ARGUMENTS
    #elif MAX_ARGUMENTS > ULONG_MAX
    #error "Cannot define MAX_ARGUMENTS to be greater than " #ULONG_MAX " and it was " #MAX_ARGUMENTS
    #endif
#endif

#ifndef STREAM_BUFFER_LEN
#define STREAM_BUFFER_LEN 64
#else
    #if defined(ARDUINO_ARCH_AVR) && STREAM_BUFFER_LEN > UCHAR_MAX
    #error "Cannot define STREAM_BUFFER_LEN to be greater than " #UCHAR_MAX " and it was " #STREAM_BUFFER_LEN
    #elif MAX_ARGUMENTS > ULONG_MAX
    #error "Cannot define STREAM_BUFFER_LEN to be greater than " #ULONG_MAX " and it was " #STREAM_BUFFER_LEN
    #endif
#endif

#ifndef MAX_COMMANDS
#define MAX_COMMANDS 16
#else
    #if defined(ARDUINO_ARCH_AVR) && MAX_COMMANDS > ULONG_MAX
    #error "Cannot define MAX_COMMANDS to be greater than " #UCHAR_MAX " and it was " #MAX_COMMANDS
    #elif MAX_COMMANDS > ULONG_MAX
    #error "Cannot define MAX_COMMANDS to be greater than " #ULONG_MAX " and it was " #MAX_COMMANDS
    #endif
#endif

#if defined(ARDUINO_ARCH_AVR)
#define _smart_comm_size_t uint8_t
#else
#define _smart_comm_size_t uint32_t
#endif

/*
 * This library (short for Smart Communication) was designed to provide a solution to inter device communication, where this
 * device acts as a child, only reacting to the commands given to it by a parent. The communication is done via messages
 * (ended by endChar) that consist of a command, and arguments (separated by sepChar). The communication can occur over any
 * protocol that implements Stream. A message looks like this:
 *     pump 250 50 true
 * The first word is always the command, and the rest are the arguments passed to that command. In this example we can send such
 * a message if we want our device to pump water (command) for 250 milliseconds (first argument), with 50% flowrate (second argument),
 * and we want to disengage the pump afterward (third argument). The separation characters and message-end characters can be changed
 * to whatever you want. Their defaults are sepChar=' ' and endChar='\n'.
 * A This library consists of three classes:
 * 
 * SmartComm:
 * This is the main class that manages comunication. At the begining of the program it should be provided with all the commands
 * and calling the method "tick()" regularly allows the library to process any incoming messages in the way specified at the
 * beginning of the program
 * 
 * SmartCmd:
 * This is the class that represents a command. it holds the informaition of the command itself as a string, and a pointer to a
 * callback that should be executed if that particular command is received
 * 
 * SmartCmdArguments:
 * This class is passed by pointer to the callbacks of the commands. These classes provide access to the arguments that were
 * received in the message alongside the command.
 * 
 * Of note is the structure of the callback. The function type is void (*smartCmdCB_t)(Stream*, const SmartCmdArguments*, const char*) which means
 * a function like:
 *     void cmd(Stream *stream, const SmartCmdArguments *args, const char *cmd) {
 *         ...
 *     }
 * will work. These functions are paired with commands on the SmartCmd classes and are called each time the corresponding
 * command is received. The stream argument can be used to send back relevant information to the sender through the same
 * communication interface that received the message. The agrs argument is a pointer to a SmartCmdArguments class, which
 * holds the information of the arguments paired with the command.
 * 
 * This library was written with memory footprint in mind and tries to reuse all buffers in a way the least amount of memory is
 * used at the same time. Because of this it could handle long messages and many different commands simultaneously. It was also
 * designed to allocate no memory on the heap (except when using PROGMEM variables where copying the PROGMEM data too RAM is
 * non-avoidable). Because of this the setup of the commands and callbacks may be a bit more involve than you might expect.
 * However you'll see it's quite intuitive.
 * 
 * You can use the helper macro SMART_CMD_CREATE to more easily create commands. The macro is used in the following way.
 * SMART_CMD_CREATE(className, command, callback);
 * where <className> is the literal name the SmartCmd class will have. <command> is the literal string surrounded by quotes ("")
 * for the SmartCmd. <callback> is the callback function for the SmartCmd (previously described). This macro can be specially helpful
 * if the target board can take advantage of PROGMEM variables. These are variables that are stored in flash instead of RAM, reducing
 * the requirements for RAM in your project. If PROGMEM is not available, traditional variables will be used. The literal code that
 * is generated by using SMART_CMD_CREATE(className, "command", callback); when PROGMEM is available is the following:
 *
 * const PROGMEM char __className_pstr_cmd[] = "command";
 * SmartCmdF className(__className_pstr_cmd, callback);
 * 
 * or, if no PROGMEM is available:
 *
 * SmartCmd className("command", callback);
 *
 * So, in the case where PROGMEM is available, we don't need to concern ourselves with creating and managing the PROGMEM variable,
 * which are automatically created for us, and then plugged into a new SmartCmdF class with the provided name (this class is the same
 * as SmartCmd but can deal with PROGMEM variables). If PROGMEM is not available, it falls back to a completely normal creation of
 * a CmartCmd class.
 *
 * An example of using the helper macro
 * is the following:
 *
 * SMART_CMD_CREATE(cmdOK, "OK", [](Stream *stream, const SmartCmdArguments *args, const char *cmd){
 *     stream->println("OK");
 * });
 * 
 * here I used a lambda function, but the same can be accomplished with normal functions
 *
 * void cmdOkCb(Stream *stream, const SmartCmdArguments *args, const char *cmd) {
 *     stream->println("OK");
 * }
 * SMART_CMD_CREATE(cmdOK, "OK", cmdOkCb);
 *
 * You can always force the library to store variables in RAM even if PROGMEM is available by using SMART_CMD_CREATE_RAM instead
 * of SMART_CMD_CREATE. It can reduce the RAM memory footprint when the commands are few and are very short strings. Because using
 * one or the other is as simple as replacing SMART_CMD_CREATE for SMART_CMD_CREATE_RAM or viceversa I encourage you to try the one
 * that fits best your project (Just in case you need a tie-breaker, PROGMEM variables might be a bit slower to work with)
 */

// #define _SMART_COMM_DEBUG


#ifdef _SMART_COMM_DEBUG
#define _SMART_COMM_DEBUG_PRINT(msg) Serial.print(msg);
#else
#define _SMART_COMM_DEBUG_PRINT(msg)
#endif

#ifdef F
#define _SMART_COMM_DEBUG_PRINT_STATIC(msg) _SMART_COMM_DEBUG_PRINT(F(msg))
#else
#define _SMART_COMM_DEBUG_PRINT_STATIC(msg) _SMART_COMM_DEBUG_PRINT(msg)
#endif


#ifdef _SMART_COMM_DEBUG
    #ifdef PROGMEM
    #pragma message ( "PROGMEM is available!" )
    #else
    #pragma message ( "PROGMEM is NOT available!" )
    #endif
#endif

/// SmartCmdArguments /////////////////////////////////////////////////////////////////////////

struct SmartCmdArguments
{
private:
    const char *const *const _args;
    const char *arg(_smart_comm_size_t n) const;

public:
    const _smart_comm_size_t N = 0;

    SmartCmdArguments(_smart_comm_size_t n, const char *const args[MAX_ARGUMENTS]);
    // bool toInt(_smart_comm_size_t n, long *i);
    // bool toBool(_smart_comm_size_t n, bool *b);
    template <typename T>
    bool to(_smart_comm_size_t n, T *t) const;
};


/// SmartCmds /////////////////////////////////////////////////////////////////////////////////

typedef void (*smartCmdCB_t)(Stream*, const SmartCmdArguments*, const char*);
typedef void (*serialDefaultCmdCB_t)(Stream*, const char*);

void __defaultCommandNotRecognizedCB(Stream *stream, const char *cmd);

class SmartCmdBase
{
protected:
    const char *_cmd;
    smartCmdCB_t _cb;
public:
    SmartCmdBase(const char *command, smartCmdCB_t callback);
    virtual bool is_command(const char *str) const = 0;
    virtual void callback(Stream *stream, const SmartCmdArguments *args) const = 0;
};

class SmartCmd : public SmartCmdBase
{
public:
    SmartCmd(const char *command, smartCmdCB_t callback);
    bool is_command(const char *str) const;
    void callback(Stream *stream, const SmartCmdArguments *args) const;
};

#ifdef PROGMEM
class SmartCmdF : public SmartCmdBase
{
public:
    SmartCmdF(const PROGMEM char *command, smartCmdCB_t callback);
    bool is_command(const char *str) const;
    void callback(Stream *stream, const SmartCmdArguments *args) const;
};
#endif

#define SMART_CMD_CREATE_RAM(className, command, callback) \
    SmartCmd className(command, callback);

#ifdef PROGMEM
#define SMART_CMD_CREATE(className, command, callback) \
    const PROGMEM char __##className##_pstr_cmd[] = command; \
    SmartCmdF className(__##className##_pstr_cmd, (callback));
#else
#define SMART_CMD_CREATE(className, command, callback) SMART_CMD_CREATE_RAM(className, command, (callback))
#endif

/// SmartComm /////////////////////////////////////////////////////////////////////////////////////

// void __trimChar(char *&str, char c);
// void __removeConsecutiveDuplicates(char *&str, char c);
// bool __isCharUnwanted(char c, char endChar, char sepChar);
// void __removeUnwantedChars(char *&str, char endChar, char sepChar);
bool __extractArguments(char *buffer, char endChar, char sepChar, char *&command, char *args[MAX_ARGUMENTS], _smart_comm_size_t &nArgs);

template
<_smart_comm_size_t N_CMDS>
class SmartComm
{
private:
    Stream *const _stream = nullptr;
    const SmartCmdBase *const *const _cmds;
    serialDefaultCmdCB_t _defaultCB;
    const char _endChar, _sepChar;
    char _buffer[STREAM_BUFFER_LEN+1] = {'\0'};
    _smart_comm_size_t _bufferPos = 0;

public:
    constexpr SmartComm(const SmartCmdBase *const cmds[N_CMDS], Stream &stream=Serial, char endChar = '\n', char sepChar = ' ', serialDefaultCmdCB_t defaultCB=__defaultCommandNotRecognizedCB);
    void tick();
};

template<_smart_comm_size_t N_CMDS>
constexpr SmartComm<N_CMDS>::SmartComm(const SmartCmdBase *const cmds[N_CMDS], Stream &stream, char endChar, char sepChar, serialDefaultCmdCB_t defaultCB)
: _cmds(cmds), _stream(&stream), _defaultCB(defaultCB), _endChar(endChar), _sepChar(sepChar)
{
    static_assert(N_CMDS <= MAX_COMMANDS, "Can't have this many commands");
}

template<_smart_comm_size_t N_CMDS>
void SmartComm<N_CMDS>::tick()
{
    while (_stream->available())
    {
        char c = _stream->read();

        if (c == _endChar)
        {
            _SMART_COMM_DEBUG_PRINT_STATIC("SMARTCOMM DEBUG: Processing message: '");_SMART_COMM_DEBUG_PRINT(_buffer);_SMART_COMM_DEBUG_PRINT_STATIC("'\n");

            char *command, *args[MAX_ARGUMENTS] = {0};
            _smart_comm_size_t nArgs = 0;

            if (__extractArguments(_buffer, _endChar, _sepChar, command, args, nArgs))
            {
                // get the serial command selected
                const SmartCmdBase *sc = NULL;
                for (_smart_comm_size_t i = 0; i < N_CMDS; i++)
                {
                    if (!_cmds[i])
                        // to prevent the edge case where the user specifies N_CMDS to be a greater number than the provided commands in the cmds array
                        // or when the cmds array has nullprts inside
                        continue;
                    if (_cmds[i]->is_command(command))
                    {
                        sc = _cmds[i];
                        break;
                    }
                }

                if (sc)
                {
                    _SMART_COMM_DEBUG_PRINT_STATIC("SMARTCOMM DEBUG: Found SmartCmd for command '");_SMART_COMM_DEBUG_PRINT(command);_SMART_COMM_DEBUG_PRINT_STATIC("'. Calling the SmartCmd callback\n");
                    // execute command
                    const SmartCmdArguments smartArgs(nArgs, args);
                    sc->callback(_stream, &smartArgs);
                }
                else
                {
                    // execute default command
                    _SMART_COMM_DEBUG_PRINT_STATIC("SMARTCOMM DEBUG: Coundln't find SmartCmd for command '");_SMART_COMM_DEBUG_PRINT(command);_SMART_COMM_DEBUG_PRINT_STATIC("'. Calling default callback\n");
                    _defaultCB(_stream, command);
                }
            }

            memset(_buffer, '\0', STREAM_BUFFER_LEN);
            _bufferPos = 0;
        }
        else
        {
            if (_bufferPos < STREAM_BUFFER_LEN-1)
                _buffer[_bufferPos++] = c;
            else
            {
                // forget old inputs (but slow)
                _SMART_COMM_DEBUG_PRINT_STATIC("SMARTCOMM DEBUG: Buffer input overflowing");
                memmove(_buffer+1, _buffer, STREAM_BUFFER_LEN-1);
                _buffer[STREAM_BUFFER_LEN-2] = c;
            }
        }
    }
}


#endif /* _SIMPLE_COMM_H_ */