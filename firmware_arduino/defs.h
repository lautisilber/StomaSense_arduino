#ifndef _DEFS_H_
#define _DEFS_H_

#define LED_PIN 22

#define BME280_SDA_PIN 8
#define BME280_SCL_PIN 9

#define HX711_SCK      16
#define HX711_DT       17
#define HX711_MULT_1   5
#define HX711_MULT_2   6
#define HX711_MULT_3   7
#define HX711_MULT_4   4

#define SERVO_PIN      11
// #define SERVO_MIN_DUTY 800
// #define SERVO_MAX_DUTY 2200

#define PUMP_PIN       10

#define A2_PIN         28
#define A3_PIN         29

#define SD_CS   1
#define SD_MISO 0
#define SD_MOSI 3
#define SD_SCK  2

#define STEPPER_PIN_1 15
#define STEPPER_PIN_2 14
#define STEPPER_PIN_3 13
#define STEPPER_PIN_4 12




#define STREAM_BUFFER_LEN 4096




#define SERVO_MIN_ANGLE 1
#define SERVO_MAX_ANGLE 179
#define SERVO_REAL_MIN_ANGLE 0
#define SERVO_REAL_MAX_ANGLE 180
#define SERVO_SLOW_DEFAULT_STEPS_PER_ANGLE 5.0
#define SERVO_SLOW_DEFAULT_DELAY_US_PER_ANGLE 100




#define pwrtwo(x) (1 << (x))
#define N_MULTIPLEXER_PINS 4
#define N_MULTIPLEXERS pwrtwo(N_MULTIPLEXER_PINS)



#define HX_STATS_N 30

#endif /* _DEFS_H_ */