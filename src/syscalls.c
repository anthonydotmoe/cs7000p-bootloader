#include <stddef.h>
#include "tusb.h"

int _write(int fd, const void *buf, size_t count) {
    (void) fd;
    tud_cdc_write(buf, count);
    tud_cdc_write_flush();
    return count;
}
