#ifndef _EEPROM_HELPER_H_
#define _EEPROM_HELPER_H_

#include <Arduino.h>
#include <EEPROM.h>

#include "debug_helper.h"

// https://arduino-pico.readthedocs.io/en/latest/eeprom.html#eeprom-examples%5B/url%5D

#define RP2040_FLASH_PAGE_SIZE 256

namespace EEPROM_Helper
{
    // has to be extern, otherwise each time this header file is included in a source file (any cpp file), the copmiler will try to redefine _eeprom_init_flag, which is a no-no.
    // instead we define it once in a source file and access it through the header file as an extern variable
    extern bool _eeprom_init_flag;

    template <typename T>
    inline void begin()
    {
        if (_eeprom_init_flag) return;
        constexpr size_t type_size = sizeof(T);
        constexpr size_t min_pages = (type_size / (size_t)RP2040_FLASH_PAGE_SIZE) + (type_size % RP2040_FLASH_PAGE_SIZE > 0);
        constexpr size_t eeprom_size = RP2040_FLASH_PAGE_SIZE * min_pages;
        EEPROM.begin(eeprom_size);
        _eeprom_init_flag = true;
    }

    template <typename T>
    bool get(T &t)
    {
        if (!_eeprom_init_flag)
        {
            ERROR_PRINTLN("EEPROM_Helper init flag is not set");
            return false;
        }
        EEPROM.get(0, t);
        return true;
    }

    template <typename T>
    bool put(T &t)
    {
        if (!_eeprom_init_flag)
        {
            ERROR_PRINTLN("EEPROM_Helper init flag is not set");
            return false;
        }
        EEPROM.put(0, t);
        return true;
    }

    inline bool commit()
    {
        return EEPROM.commit();
    }
}



#endif /* _EEPROM_HELPER_H_ */