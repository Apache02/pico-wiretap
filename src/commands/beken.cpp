#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "shell/Parser.h"
#include "beken/bootloader_7252_uart2_crc.h"

#define BEKEN_FLASH_DO    (12)
#define BEKEN_FLASH_DI    (11)
#define BEKEN_FLASH_CLK   (10)
#define BEKEN_FLASH_CE    (9)
#define BEKEN_CEN         (17)

#define spi_instance      spi1
#define DEFAULT_BAUDRATE  (50000)

#define FLASH_CHIP_ID           0x9F
#define FLASH_STATUS_WR_LOW     0x01
#define FLASH_STATUS_WR_HIGH    0x31
#define FLASH_WRITE_PAGE        0x02
#define FLASH_READ              0x03
#define FLASH_ENABLE_WRITE      0x06
#define FLASH_ERASE_SECTOR      0x20
#define FLASH_WAIT_ERASE        0x05
#define FLASH_ERASE_CHIP        0xc7


#define cs_0()          gpio_put(BEKEN_FLASH_CE, false)
#define cs_1()          gpio_put(BEKEN_FLASH_CE, true)
#define cen_0()         gpio_put(BEKEN_CEN, false)
#define cen_1()         gpio_put(BEKEN_CEN, true)

// debug
// #define DEBUG
#ifdef DEBUG
#include "shell/console_colors.h"
static void print_(const char *prefix, uint8_t *buf, size_t length) {
    bool is_empty = true;
    printf("%s: ", prefix);
    for (int i = 0; i < length; i++) {
        printf("%02X ", buf[i]);
        is_empty = is_empty && (buf[i] == 0);
    }
    if (is_empty) printf(COLOR_RED("(empty)"));
    printf("\r\n");
}

#define print_tx(buf, size)         print_(COLOR_YELLOW("TX"), buf, size)
#define print_rx(buf, size)         print_(COLOR_GREEN("RX"), buf, size)
#else
#define print_tx(buf, size)         ((void) 0)
#define print_rx(buf, size)         ((void) 0)
#endif


static void spi_flash_init(int baudrate) {
    gpio_init(BEKEN_FLASH_CE);
    gpio_set_dir(BEKEN_FLASH_CE, GPIO_OUT);
    gpio_put(BEKEN_FLASH_CE, 1);

    gpio_init(BEKEN_CEN);
    gpio_set_dir(BEKEN_CEN, GPIO_OUT);
    gpio_put(BEKEN_CEN, 1);

    spi_init(spi_instance, baudrate);
    spi_set_format(spi_instance, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(BEKEN_FLASH_DO, GPIO_FUNC_SPI);
    gpio_set_function(BEKEN_FLASH_DI, GPIO_FUNC_SPI);
    gpio_set_function(BEKEN_FLASH_CLK, GPIO_FUNC_SPI);
}

static void beken_reset(uint32_t delay) {
    cen_0();
    sleep_ms(delay);
    cen_1();
}

static bool beken_enter_spi_flash(uint32_t start_delay) {
    cs_1();
    beken_reset(100);
    cs_0();

    sleep_ms(start_delay);

    uint8_t tx[2] = {0xD2, 0xD2};
    uint8_t rx[2] = {0, 0};
    spi_write_read_blocking(spi_instance, tx, rx, 2);
    cs_1();

    if (rx[0] != 0xD2) {
        printf("handshake fail: got %02X %02X\n", rx[0], rx[1]);
        return false;
    }

    sleep_ms(1);

    cs_0();
    uint8_t rx_buf[1024];
    size_t rx_size = 0;
    rx_size += spi_read_blocking(spi_instance, 0, &rx_buf[rx_size], 2);
    bool success = false;

    while (rx_size < count_of(rx_buf) && !success) {
        int size = spi_read_blocking(spi_instance, FLASH_CHIP_ID, &rx_buf[rx_size], 1);
        int rx_byte = rx_buf[rx_size];
        rx_size += size;
        if (rx_byte != 0 && rx_byte != 0xD2) {
            success = true;
        }
    }
    cs_1();

    print_tx(tx, 2);
    print_rx(rx, 2);
    print_rx(rx_buf, rx_size);

    return success;
}

static bool flash_read_chip_id(uint8_t out[3]) {
    uint8_t cmd[4] = {FLASH_CHIP_ID, 0, 0, 0};
    uint8_t rx[4];

    for (int i = 0; i < 500; i++) {
        cs_0();
        spi_write_read_blocking(spi_instance, cmd, rx, 4);
        cs_1();

        print_tx(cmd, sizeof(cmd));
        print_rx(rx, sizeof(rx));

        if (rx[1] | rx[2] | rx[3]) {
            out[0] = rx[1];
            out[1] = rx[2];
            out[2] = rx[3];
            return true;
        }
    }
    return false;
}

static uint8_t flash_read_sr() {
    uint8_t tx[2] = {FLASH_WAIT_ERASE, 0};
    uint8_t rx[2] = {0, 0};
    cs_0();
    spi_write_read_blocking(spi_instance, tx, rx, 2);
    cs_1();
    return rx[1];
}

static void flash_wait_wip() {
    while (flash_read_sr() & 0x01) {
        sleep_ms(10);
    }
}

static void flash_enable_write() {
    uint8_t tx[] = {FLASH_ENABLE_WRITE};
    cs_0();
    spi_write_blocking(spi_instance, tx, sizeof(tx));
    cs_1();
}

static bool flash_erase_chip() {
    uint8_t sr = flash_read_sr();
    printf("SR: 0x%02X\r\n", sr);

    if (sr & 0x1C) {
        flash_enable_write();
        flash_wait_wip();

        uint8_t wrsr[] = {FLASH_STATUS_WR_LOW, 0x00};
        cs_0();
        spi_write_blocking(spi_instance, wrsr, sizeof(wrsr));
        cs_1();
        flash_wait_wip();

        sr = flash_read_sr();
        if (sr & 0x1C) {
            printf("clear BP failed: SR=0x%02X\r\n", sr);
            return false;
        }
    }

    flash_enable_write();

    uint8_t ce = 0xC7;
    cs_0();
    spi_write_blocking(spi_instance, &ce, 1);
    cs_1();

    printf("erasing");
    int tick = 0;
    while (flash_read_sr() & 0x01) {
        sleep_ms(500);
        if (++tick % 4 == 0)
            printf(".");
    }
    printf(" done\r\n");
    return true;
}

static void flash_dump(uint32_t addr) {
    uint8_t cmd[4] = {
        FLASH_READ,
        static_cast<uint8_t>((addr >> 16) & 0xFF),
        static_cast<uint8_t>((addr >> 8) & 0xFF),
        static_cast<uint8_t>((addr >> 0) & 0xFF)
    };
    uint8_t data[256];

    cs_0();
    spi_write_blocking(spi_instance, cmd, sizeof(cmd));
    spi_read_blocking(spi_instance, 0x00, data, sizeof(data));
    cs_1();

    for (int i = 0; i < 256; i += 16) {
        printf("%06X:  ", addr + i);
        for (int j = 0; j < 16; j++)
            printf("%02X ", data[i + j]);
        printf(" |");
        for (int j = 0; j < 16; j++) {
            uint8_t c = data[i + j];
            printf("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
        }
        printf("|\r\n");
    }
}

static void flash_erase_sector(uint32_t addr) {
    uint8_t tx[4] = {
        FLASH_ERASE_SECTOR,
        static_cast<uint8_t>((addr >> 16) & 0xFF),
        static_cast<uint8_t>((addr >> 8) & 0xFF),
        static_cast<uint8_t>((addr >> 0) & 0xFF)
    };

    cs_0();
    spi_write_blocking(spi_instance, tx, sizeof(tx));
    cs_1();

    flash_wait_wip();
}

static void flash_write_256(uint32_t addr, const uint8_t *src) {
    uint8_t tx[4] = {
        FLASH_WRITE_PAGE,
        static_cast<uint8_t>((addr >> 16) & 0xFF),
        static_cast<uint8_t>((addr >> 8) & 0xFF),
        static_cast<uint8_t>((addr >> 0) & 0xFF)
    };

    cs_0();
    spi_write_blocking(spi_instance, tx, sizeof(tx));
    spi_write_blocking(spi_instance, src, 256);
    cs_1();

    flash_wait_wip();
}

static void flash_write(uint32_t addr, const uint8_t *src, size_t size) {
    for (; addr < size;) {
        if ((addr & 0xFFF) == 0) {
            flash_enable_write();
            flash_erase_sector(addr);
        }

        printf("write %08x ... ", addr);
        flash_enable_write();
        flash_write_256(addr, (src + addr));
        addr += 256;
        printf("done\n");
    }

    flash_enable_write();

    uint8_t wrsr[] = {FLASH_STATUS_WR_LOW, 0x00};
    cs_0();
    spi_write_blocking(spi_instance, wrsr, sizeof(wrsr));
    cs_1();

    flash_wait_wip();
}

static void usage() {
    printf("Pins: TDO: %d  TDI: %d  CLK:%d  TCS: %d | CEN: %d\n\n",
           BEKEN_FLASH_DO,
           BEKEN_FLASH_DI,
           BEKEN_FLASH_CLK,
           BEKEN_FLASH_CE,
           BEKEN_CEN
    );

    printf("usage:\n");
    printf("  beken <subcommand> [... subcommands] [options]\n");
    printf("\n");
    printf("options:\n");
    printf("  --baudrate, -b            baudrate, default = %d\n", DEFAULT_BAUDRATE);
    printf("  --delay                   start delay, default = %d\n", 5);
    printf("\n");
    printf("subcommands:\n");
    printf("  reset                     enter SPI flash mode\n");
    printf("  id                        read chip ID\n");
    printf("  erase                     chip erase\n");
    printf("  dump <addr>               hex dump 256 bytes\r\n");
}

int command_beken(int argc, const char *argv[]) {
    printf("\r\n");

    if (argc < 2) {
        usage();
        return 0;
    }

    int baudrate = DEFAULT_BAUDRATE;
    int start_delay = 5;
    bool help = false;

    // parse options
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') continue;

        if (
            strcmp(argv[i], "--baudrate") == 0
            || strcmp(argv[i], "-b") == 0
        ) {
            if (i + 1 >= argc) {
                printf("Error: invalid option baudrate\n");
                return -1;
            }

            baudrate = take_int(argv[i + 1]).ok_or(DEFAULT_BAUDRATE);
        } else if (strcmp(argv[i], "--delay") == 0) {
            if (i + 1 >= argc) {
                printf("Error: invalid option delay\n");
                return -1;
            }
            start_delay = take_int(argv[i + 1]).ok_or(start_delay);
        } else if (strcmp(argv[i], "--help") == 0) {
            help = true;
        }
    }

    if (help) {
        usage();
        return 0;
    }

    printf("* %s (%d)\n", "init spi", baudrate);
    spi_flash_init(baudrate);

    // actions
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if ((i + 1 < argc) && (argv[i + 1][0] >= '0') && (argv[i + 1][0] <= '9')) {
                i++;
            }
            continue;
        };

        if (strcmp(argv[i], "reset") == 0) {
            printf("* %s (%d)\n", argv[i], start_delay);
            if (!beken_enter_spi_flash(start_delay)) {
                printf("Error: enter spi flash error\n");
                return 1;
            }
            printf("SPI flash mode OK\n");
        } else if (strcmp(argv[i], "id") == 0) {
            printf("* %s\n", argv[i]);

            uint8_t id[3];
            if (!flash_read_chip_id(id)) {
                printf("Error: read chip id error\n");
                return 1;
            }

            printf("chip ID: %02X %02X %02X\r\n", id[0], id[1], id[2]);
        } else if (strcmp(argv[i], "dump") == 0) {
            if (i + 1 >= argc) {
                printf("Error: address required\n");
                return -1;
            }
            int addr = take_int(argv[i + 1]).ok_or(-1);
            if (addr < 0) {
                printf("Error: invalid address\n");
                return -1;
            }

            printf("* %s(0x%x)\n", argv[i], addr);
            i++;

            flash_dump(static_cast<uint32_t>(addr));
        } else if (strcmp(argv[i], "erase") == 0) {
            printf("* %s\n", argv[i]);
            printf("  %s\n", flash_erase_chip() ? "OK" : "Fail");
        } else if (strcmp(argv[i], "write_bootloader") == 0) {
            printf("* %s\n", argv[i]);
            flash_write(0x00000000, bootloader_crc_bin, bootloader_crc_bin_len);
            printf("  done\n");
        } else {
            printf("unknown subcommand: %s\n", argv[i]);
        }
    }

    return 0;
}
