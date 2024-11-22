#ifndef _POLLING_TIMER_H_
#define _POLLING_TIMER_H_

#include <Arduino.h>
#include <limits.h>

namespace PollingTimer
{
    inline bool timer_finished_ms(unsigned int begin_time_ms, unsigned int total_time_ms)
    {
        // https://arduino.stackexchange.com/questions/12587/how-can-i-handle-the-millis-rollover/12588#12588
        // https://www.norwegiancreations.com/2018/10/arduino-tutorial-avoiding-the-overflow-issue-when-using-millis-and-micros/
        unsigned int duration_ms = millis() - begin_time_ms;
        return total_time_ms > total_time_ms;
    }
}

#endif /* _POLLING_TIMER_H_ */