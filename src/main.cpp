#include <stdio.h>
#include "pico/stdio.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include <FreeRTOS.h>
#include <task.h>
#include "tusb.h"

#include "usb/usb_task.h"
#include "shell_task.h"
#include "uart/uart_task.h"


int transfer_flag = 0;

#ifdef PICO_DEFAULT_LED_PIN

void vTaskLed(__unused void *pvParams) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    for (;;) {
        gpio_put(PICO_DEFAULT_LED_PIN, is_usb_connected() xor (transfer_flag != 0));
        transfer_flag = 0;
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
    xTaskCreate(vTaskLed, "led",configMINIMAL_STACK_SIZE, nullptr, 1, nullptr);
#endif

    TaskHandle_t xhTaskUsb;
    xTaskCreate(vTaskUsb, "usb",configMINIMAL_STACK_SIZE * 2, nullptr,configMAX_PRIORITIES - 1, &xhTaskUsb);
    vTaskCoreAffinitySet(xhTaskUsb, 1 << 0);

    TaskHandle_t xhTaskShell;
    xTaskCreate(vTaskShell, "shell",configMINIMAL_STACK_SIZE * 4, nullptr,configMAX_PRIORITIES - 2, &xhTaskShell);
    vTaskCoreAffinitySet(xhTaskShell, 1 << 0);

    unsigned int uUartNumber0 = 0, uUartNumber1 = 1;
    TaskHandle_t xhTaskUart0, xhTaskUart1;
    xTaskCreate(vTaskUart, "uart0",configMINIMAL_STACK_SIZE * 8, &uUartNumber0,configMAX_PRIORITIES - 1, &xhTaskUart0);
    xTaskCreate(vTaskUart, "uart1",configMINIMAL_STACK_SIZE * 8, &uUartNumber1,configMAX_PRIORITIES - 1, &xhTaskUart1);
    vTaskCoreAffinitySet(xhTaskUart0, 1 << 1);
    vTaskCoreAffinitySet(xhTaskUart1, 1 << 1);

    vTaskStartScheduler();

    return 0;
}
