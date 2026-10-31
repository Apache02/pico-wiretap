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
#include "plot/plot_task.h"


volatile int transfer_flag = 0;

#ifdef PICO_DEFAULT_LED_PIN

static StaticTask_t xLedTaskTCB;
static StackType_t  xLedTaskStack[configMINIMAL_STACK_SIZE];
static void vTaskLed(__unused void *pvParams) {
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    for (;;) {
        gpio_put(PICO_DEFAULT_LED_PIN, is_usb_connected() xor (transfer_flag != 0));
        transfer_flag = 0;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

#endif

static StaticTask_t xUart0TaskTCB, xUart1TaskTCB;
static StackType_t  xUart0TaskStack[configMINIMAL_STACK_SIZE * 8];
static StackType_t  xUart1TaskStack[configMINIMAL_STACK_SIZE * 8];

void init_hardware() {
    set_sys_clock_khz(configCPU_CLOCK_HZ / 1000, false);
    usbd_serial_init();
    tusb_init();
    stdio_usb_init();
}

int main() {
    init_hardware();

#ifdef PICO_DEFAULT_LED_PIN
    xTaskCreateStatic(vTaskLed, "led", count_of(xLedTaskStack), nullptr, 1, &xLedTaskStack[0], &xLedTaskTCB);
#endif

    TaskHandle_t xhTaskUsb;
    xTaskCreate(vTaskUsb, "usb",configMINIMAL_STACK_SIZE * 2, nullptr,configMAX_PRIORITIES - 1, &xhTaskUsb);
    vTaskCoreAffinitySet(xhTaskUsb, 1 << 0);

    TaskHandle_t xhTaskShell;
    xTaskCreate(vTaskShell, "shell",configMINIMAL_STACK_SIZE * 4, nullptr,configMAX_PRIORITIES - 2, &xhTaskShell);
    vTaskCoreAffinitySet(xhTaskShell, 1 << 0);

    // The UART number travels in the pointer itself: main()'s stack is reclaimed for
    // interrupts once the scheduler starts, before the tasks would read through it.
    // Pinned from creation: each task enables its UART IRQ on the core it runs on.
    xTaskCreateStaticAffinitySet(vTaskUart, "uart0", count_of(xUart0TaskStack), reinterpret_cast<void *>(0), configMAX_PRIORITIES - 1, &xUart0TaskStack[0], &xUart0TaskTCB, 1 << 1);
    xTaskCreateStaticAffinitySet(vTaskUart, "uart1", count_of(xUart1TaskStack), reinterpret_cast<void *>(1), configMAX_PRIORITIES - 1, &xUart1TaskStack[0], &xUart1TaskTCB, 1 << 1);

    xTaskCreate(vTaskPlot, "plot",configMINIMAL_STACK_SIZE * 8, nullptr,configMAX_PRIORITIES - 1, &xhTaskPlot);

    vTaskStartScheduler();

    return 0;
}
