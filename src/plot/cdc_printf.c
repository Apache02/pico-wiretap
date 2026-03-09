#include "cdc_printf.h"
#include <string.h>
#include <stdarg.h>
#include "tusb.h"

extern int transfer_flag;

void cdc_puts(uint8_t itf, const char *s) {
    transfer_flag = 1;
    tud_cdc_n_write(itf, s, strlen(s));
    tud_cdc_n_write_flush(itf);
}

void cdc_printf(uint8_t itf, const char *fmt, ...) {
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    cdc_puts(itf, buf);
}
