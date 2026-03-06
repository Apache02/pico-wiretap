#include "shell/Parser.h"

#include <stdio.h>
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#define I2C_PIN_SDA     2
#define I2C_PIN_SCL     3
#undef I2C_INSTANCE
#define I2C_INSTANCE    i2c1
#define DEFAULT_BAUDRATE (100 * 1000)


static bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

int command_i2c_scan(int argc, const char *argv[]) {
    auto baudrate = take_int(argv[1]).ok_or(DEFAULT_BAUDRATE);

    gpio_init(I2C_PIN_SDA);
    gpio_init(I2C_PIN_SCL);
    gpio_set_function(I2C_PIN_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_PIN_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_PIN_SDA);
    gpio_pull_up(I2C_PIN_SCL);

    i2c_init(I2C_INSTANCE, baudrate);

    printf("\n");
    printf("SDA: GP%d  SCL: GP%d\n", I2C_PIN_SDA, I2C_PIN_SCL);
    printf("Baudrate: %d\n", baudrate);

    printf("\n   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }

        int ret;
        uint8_t txdata = 0;
        if (reserved_addr(addr)) {
            ret = PICO_ERROR_GENERIC;
        } else {
            ret = i2c_write_blocking(I2C_INSTANCE, addr, &txdata, 1, false);
        }

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }

    gpio_init(I2C_PIN_SDA);
    gpio_init(I2C_PIN_SCL);
    gpio_disable_pulls(I2C_PIN_SDA);
    gpio_disable_pulls(I2C_PIN_SCL);

    return 0;
}