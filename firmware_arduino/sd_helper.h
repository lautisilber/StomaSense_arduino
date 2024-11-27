#ifndef _SD_HELPER_H_
#define _SD_HELPER_H_

#include <Arduino.h>
#include <SD.h>

namespace SD_Helper
{
    void begin();

    bool close(File *file);

    bool open(File *file, const char *fname, int mode);
    bool open_write(File *file, const char *fname);
    bool open_append(File *file, const char *fname);
    bool open_read(File *file, const char *fname);

    bool write(const char *fname, const char *content);
    bool append(const char *fname, const char *content);
    bool read(const char *fname, char *buf, size_t buf_len);
}

#endif /* _SD_HELPER_H_ */



