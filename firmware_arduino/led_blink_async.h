#ifndef _LED_BLINK_ASYNC_H_
#define _LED_BLINK_ASYNC_H_

#include <Arduino.h>

extern bool led_blink_async(uint8_t pin, uint32_t delay_ms, bool inverted=false);


#endif /* _LED_BLINK_ASYNC_H_ */