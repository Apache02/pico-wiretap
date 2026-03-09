#include <stdio.h>
#include <string.h>
#include "shell/console_colors.h"


int command_pinout(int argc, const char *argv[]) {
    int show_all = (argc < 2);

    int show_uart = show_all || (strcmp(argv[1], "uart") == 0);
    int show_i2c = show_all || (strcmp(argv[1], "i2c") == 0);
    int show_gpio = show_all || (strcmp(argv[1], "gpio") == 0);
    int show_adc = show_all || (strcmp(argv[1], "adc") == 0);
    int show_pwm = false; // show_all || (strcmp(argv[1], "pwm") == 0);

    if (!show_uart && !show_i2c && !show_pwm && !show_gpio && !show_adc) {
        printf("Usage: pinout [uart|i2c|pwm|gpio|adc]\r\n");
        return 1;
    }

    printf("  GPIO | Function     \r\n");
    printf("  --------------------\r\n");

    if (show_uart) {
        printf("  %4d | %s\r\n", 0, COLOR_YELLOW("UART0") " TX");
        printf("  %4d | %s\r\n", 1, COLOR_YELLOW("UART0") " RX");
        printf("  %4d | %s\r\n", 4, COLOR_YELLOW("UART1") " TX");
        printf("  %4d | %s\r\n", 5, COLOR_YELLOW("UART1") " RX");
    }
    if (show_i2c) {
        printf("  %4d | %s\r\n", 2, COLOR_CYAN("I2C") " SDA");
        printf("  %4d | %s\r\n", 3, COLOR_CYAN("I2C") " SCL");
    }
    if (show_gpio) {
        printf("  %4d | %s\r\n", 14, COLOR_MAGENTA("GPIO") " monitor");
        printf("  %4d | %s\r\n", 15, COLOR_MAGENTA("GPIO") " monitor");
    }
    if (show_adc) {
        printf("  %4d | %s\r\n", 26, COLOR_BLUE("ADC0") " monitor");
        printf("  %4d | %s\r\n", 27, COLOR_BLUE("ADC1") " monitor");
        printf("  %4d | %s\r\n", 28, COLOR_BLUE("ADC2") " monitor");
    }
    if (show_pwm) {
        printf("  %4d | %s\r\n", 6, COLOR_GREEN("PWM") " capture");
        printf("  %4d | %s\r\n", 7, COLOR_GREEN("PWM") " capture");
        printf("  %4d | %s\r\n", 12, COLOR_GREEN("PWM") " capture");
        printf("  %4d | %s\r\n", 13, COLOR_GREEN("PWM") " capture");
    }

    return 0;
}
