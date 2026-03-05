#include <stdio.h>
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include <FreeRTOS.h>
#include <task.h>
#include "tusb.h"

#include "usb/usb_task.h"
#include "shell_task.h"


#ifdef PICO_DEFAULT_LED_PIN

void vTaskLed(void *pvParams) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    for (;;) {
        gpio_put(PICO_DEFAULT_LED_PIN, usb_is_connected());
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

#endif

void init_hardware() {
    set_sys_clock_khz(configCPU_CLOCK_HZ / 1000, false);
    usbd_serial_init();
    tusb_init();
    stdio_usb_init();
}

int main() {
    init_hardware();

#ifdef PICO_DEFAULT_LED_PIN
    xTaskCreate(
            vTaskLed,
            "led",
            configMINIMAL_STACK_SIZE,
            NULL,
            1,
            NULL
    );
#endif

    xTaskCreate(
            vTaskUsb,
            "usb",
            configMINIMAL_STACK_SIZE * 2,
            NULL,
            1,
            NULL
    );

    xTaskCreate(
            vTaskShell,
            "shell",
            configMINIMAL_STACK_SIZE * 4,
            NULL,
            1,
            NULL
    );

    vTaskStartScheduler();

    return 0;
}
