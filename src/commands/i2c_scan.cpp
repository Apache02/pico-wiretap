#include "shell/Parser.h"

#include <stdio.h>
#include "hardware/i2c.h"
#include "hardware/gpio.h"


#define DEFAULT_BAUDRATE        (100 * 1000)

static bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

static void usage(const char *command) {
    printf("\n    Usage: %s <pin1: 0-21> <pin2: 0-21> [baudrate: default %d]\n\n", command, DEFAULT_BAUDRATE);
}


int command_i2c_scan(int argc, const char *argv[]) {
    auto pin1_r = take_int(argv[1]);
    auto pin2_r = take_int(argv[2]);
    auto baudrate = take_int(argv[3]).ok_or(DEFAULT_BAUDRATE);

    if (pin1_r.is_err() || pin2_r.is_err()) {
        printf("Error: incorrect pins\n");
        usage(argv[0]);
        return 1;
    }

    auto pin1 = (int) pin1_r;
    auto pin2 = (int) pin2_r;

    if (pin1 > 21 || pin2 > 21 || pin1 < 0 || pin2 < 0) {
        printf("Error: pins must be in range [0-21]\n");
        return 1;
    }

    if (pin1 == pin2) {
        printf("Error: pins can't be equal\n");
        return 1;
    }

    if ((pin1 & 2) != (pin2 & 2)) {
        printf("Error: pins must have same i2c instance\n");
        return 1;
    }

    gpio_init(pin1);
    gpio_init(pin2);
    gpio_set_function(pin1, GPIO_FUNC_I2C);
    gpio_set_function(pin2, GPIO_FUNC_I2C);
    gpio_pull_up(pin1);
    gpio_pull_up(pin2);

    auto instance = i2c_get_instance((pin1 >> 1) & 1);
    i2c_init(instance, baudrate);

    printf("\n");
    printf("Pins: %d + %d\n", pin1, pin2);
    printf("Instance: %s\n", instance == i2c0 ? "i2c0" : "i2c1");
    printf("Baudrate: %d\n", baudrate);

    printf("\ni2c_write_blocking test\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }

        int ret;
        uint8_t txdata = 0;
        if (reserved_addr(addr)) {
            ret = PICO_ERROR_GENERIC;
        } else {
            ret = i2c_write_blocking(instance, addr, &txdata, 1, false);
        }

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }

    // reset pins
    gpio_init(pin1);
    gpio_init(pin2);
    gpio_disable_pulls(pin1);
    gpio_disable_pulls(pin2);

    return 0;
}
