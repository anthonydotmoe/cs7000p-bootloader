#pragma once

#ifdef DEBUG_DEBUG_DEBUG
int debug_printf(const char *fmt, ...);
#define CDC_LOG(...) (void) debug_printf(__VA_ARGS__)
#else
#define CDC_LOG(...)
#endif
