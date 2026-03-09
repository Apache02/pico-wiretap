#pragma once

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

void cdc_puts(uint8_t itf, const char *s);

void cdc_printf(uint8_t itf, const char *fmt, ...);

#ifdef __cplusplus
}
#endif
