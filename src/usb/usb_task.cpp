#include <FreeRTOS.h>
#include <task.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "tusb.h"
#include "usb_itf.h"

void vTaskUsb(__unused void *pvParams) {
    for (;;) {
        tud_task();

        cdc_line_coding_t lc;

        tud_cdc_n_get_line_coding(ITF_SHELL, &lc);
        if (lc.bit_rate == PICO_STDIO_USB_RESET_MAGIC_BAUD_RATE) {

            printf("\n\nPerform reboot into the BOOTSEL\n\n");
            tud_cdc_n_write_flush(ITF_SHELL);

            reset_usb_boot(0, PICO_STDIO_USB_RESET_BOOTSEL_INTERFACE_DISABLE_MASK);
        }

        vTaskDelay(1);
    }
}

bool usb_is_connected() {
    return tud_cdc_n_connected(ITF_SHELL);
}