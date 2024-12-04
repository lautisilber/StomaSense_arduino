#ifndef _BME280_HELPER_H_
#define _BME280_HELPER_H_

#include <Arduino.h>
#include <BME280I2C.h>
#include <Wire.h>
#include <hardware/sync.h>

namespace BME_Helper {
    const BME280I2C::Settings default_settings(
        BME280::OSR_X4,
        BME280::OSR_X4,
        BME280::OSR_X4
    );
    void begin(BME280I2C *bme, pin_size_t sda_pin, pin_size_t scl_pin, const BME280I2C::Settings *settings=NULL);
    bool read(BME280I2C *bme, float *hum, float *temp, float *pres);
}

#endif