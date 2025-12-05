#ifdef DEBUG_DEBUG_DEBUG

#include <stdarg.h>
#include <stdio.h>
#include "tusb.h"

#include "debug.h"

static char dbg_buf[256];

int debug_printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(dbg_buf, sizeof(dbg_buf), fmt, ap);
    va_end(ap);

    if (n > 0) {
        tud_cdc_write(dbg_buf, (uint32_t)n);
        tud_cdc_write_flush();
    }

    return n;
}
#endif
