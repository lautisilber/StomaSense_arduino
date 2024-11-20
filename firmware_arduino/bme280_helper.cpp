#include "bme280_helper.h"

namespace BME_HELPER {
    void begin(BME280I2C *bme, pin_size_t sda_pin, pin_size_t scl_pin)
    {
        Wire.setSDA(sda_pin);
        Wire.setSCL(scl_pin);
        Wire.begin();
        bme->begin();
    }

    bool read(BME280I2C *bme, float *hum, float *temp, float *pres)
    {
        uint32_t ints = save_and_disable_interrupts(); 
        bme->read(*pres, *temp, *hum);
        restore_interrupts(ints);
        return !(*pres == NAN || *temp == NAN || *hum == NAN);
    }
}