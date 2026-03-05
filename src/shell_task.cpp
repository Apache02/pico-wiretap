#include "shell_task.h"
#include <FreeRTOS.h>
#include <task.h>
#include "tusb.h"
#include "usb_itf.h"
#include "usb_task.h"
#include "shell/Shell.h"
#include "shell/console_colors.h"


extern const Shell::Handler handlers[];


static void wait_usb() {
    vTaskDelay(pdMS_TO_TICKS(500));
    while (!is_usb_connected()) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void print_welcome() {
    printf("\n%s.\n\n", COLOR_WHITE("Pico shell is ready"));
}

void vTaskShell(__unused void *pvParams) {
    auto *shell = new Shell(handlers);

    for (;;) {
        wait_usb();
        print_welcome();

        shell->reset();
        shell->start();

        while (is_usb_connected()) {
            char rx;
            if (tud_cdc_n_read(ITF_SHELL, &rx, sizeof(rx)) > 0) {
                shell->update(rx);
            }

            vTaskDelay(5);
        }
    }
}
