#include "sd_helper.h"

#include <SPI.h>

#include "debug_helper.h"
#include "defs.h"


namespace SD_Helper
{
    void begin()
    {
        SPI.setRX(SD_MISO);
        SPI.setTX(SD_MOSI);
        SPI.setSCK(SD_SCK);
    }

    bool close(File *file)
    {
        if (file)
        {
            file->close();
            SD.end();
        }
        return true;
    }

    bool open(File *file, const char *fname, int mode)
    {
        close(file);
        if (!SD.begin(SD_CS))
        {
            ERROR_PRINTLN("Couldn't initialize SD");
            return false;
        }

        (*file) = SD.open(fname, mode);
        if (!file)
        {
            ERROR_PRINTFLN("Couldn't open file '%s' in mode %i", fname, mode);
            return false;
        }
        return true;
    }
    bool open_write(File *file, const char *fname)
    {
        return open(file, fname, O_CREAT | O_WRITE);
    }
    bool open_append(File *file, const char *fname)
    {
        return open(file, fname, O_CREAT | O_APPEND);
    }
    bool open_read(File *file, const char *fname)
    {
        return open(file, fname, O_READ);
    }

    bool write(const char *fname, const char *content)
    {
        File file;
        if (!open_write(&file, fname))
        {
            ERROR_PRINTFLN("Couldn't write file '%s'", fname);
            return false;
        }

        size_t chars_given = strlen(content);
        size_t chars_written = file.print(content);
        close(&file);
        
        if (chars_written != chars_given)
        {
            WARN_PRINTFLN("Didn't write the same number of chars given while writing file '%s'. (%u, %u)", fname, chars_written, chars_given);
            return false;
        }
        return true;
    }
    bool append(const char *fname, const char *content)
    {
        File file;
        if (!open_append(&file, fname))
        {
            ERROR_PRINTFLN("Couldn't append file '%s'", fname);
            return false;
        }

        size_t chars_given = strlen(content);
        size_t chars_written = file.print(content);
        close(&file);
        
        if (chars_written != chars_given)
        {
            ERROR_PRINTFLN("Didn't write the same number of chars given while appending file '%s'. (%u, %u)", fname, chars_written, chars_given);
            return false;
        }
        return true;
    }
    bool read(const char *fname, char *buf, size_t buf_len)
    {
        File file;
        if (!open_append(&file, fname))
        {
            ERROR_PRINTFLN("Couldn't read file '%s'", fname);
            return false;
        }

        size_t file_size = file.size();
        int res = file.read((uint8_t *)buf, buf_len-1);
        buf[buf_len-1] = '\0';
        close(&file);

        if (res < 0)
        {
            ERROR_PRINTFLN("Couldn't read file '%s'", fname);
            return false;
        }
        else if (res != file_size)
        {
            ERROR_PRINTFLN("Bytes read from file '%s' don't match the file's size. (%i, %u)", fname, res, file_size);
            return false;
        }
        return true;
    }
}