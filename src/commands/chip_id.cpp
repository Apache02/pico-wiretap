#include <stdio.h>
#include "hardware/structs/sysinfo.h"
#include "hardware/flash.h"
#include "pico/unique_id.h"


static struct {
    uint8_t manufacturer;
    uint8_t memory_type;
    uint8_t capacity;
} flash_JEDEC_ID;

static void __attribute__((constructor)) _read_flash_JEDEC_ID_on_boot() {
    uint8_t txbuf[4] = {0};
    uint8_t rxbuf[4] = {0};
    // Winbond W25Q series pdf
    // 8.2.29 Read JEDEC ID (9Fh)
    txbuf[0] = 0x9f;
    flash_do_cmd(txbuf, rxbuf, 4);

    flash_JEDEC_ID.manufacturer = rxbuf[1];
    flash_JEDEC_ID.memory_type = rxbuf[2];
    flash_JEDEC_ID.capacity = rxbuf[3];
}


struct id_name_map {
    uint32_t id;
    const char *name;
};

static const char *get_name_by_id(const struct id_name_map *map, uint32_t id) {
    for (int i = 0;; i++) {
        if (map[i].id == id || map[i].id == 0) {
            return map[i].name;
        }
    }

    return "error";
}

const struct id_name_map flash_manufacturer_map[] = {
    {0xEF, "Winbond"},
    {0x20, "Micron/ST"},
    {0xC2, "Macronix (MXIC)"},
    {0x9D, "ISSI"},
    {0x1C, "EON"},
    {0xA1, "Fudan"},
    {0x85, "Puya"},
    {0x0B, "XTX"},
    {0x68, "Boya"},
    {0xC8, "GigaDevice"},
    {0xBF, "Microchip SST"},
    {0x01, "Spansion/Cypress/Infineon"},
    {0x37, "AMIC"},
    {0x8C, "ESMT"},
    {0x5E, "Zbit"},
    {0, "unknown"},
};

const struct id_name_map flash_memory_type_map[] = {
    {0x40, "SPI NOR (Quad/Dual/Standard)"},
    {0x60, "QPI NOR Flash"},
    {0x70, "SPI NOR (high performance)"},
    {0x20, "SPI NOR (legacy)"},
    {0x25, "SPI NOR 3.3V"},
    {0x26, "SPI NOR 1.8V"},
    {0, "unknown"},
};

static const char *flash_voltage(uint8_t type) {
    switch (type) {
        case 0x26: return "1.8V";
        case 0x25: return "3.3V";
        default: return "3.3V (typical)";
    }
}

int command_chip_id(int, const char *[]) {
    uint32_t chip_id = sysinfo_hw->chip_id;
    uint16_t manufacturer = (chip_id >> 0) & 0x0fff;
    uint16_t part_id = (chip_id >> 12) & 0xffff;
    uint16_t revision = (chip_id >> 28) & 0x0f;

    printf("=== Chip ID ===\r\n");
    printf("  Raw              : 0x%08lX\r\n\r\n", chip_id);
    printf("  Manufacturer     : 0x%03X\r\n", manufacturer);
    printf("  Part ID          : 0x%04X\r\n", part_id);
    printf("  Revision         : %s\r\n", revision == 2 ? "B2" : "B0/B1");

    printf("\r\n");

    char board_id[32];
    pico_get_unique_board_id_string(board_id, sizeof(board_id));
    printf("Unique board identifier: %s\r\n", board_id);

    printf("\r\n");

    uint8_t mfr = flash_JEDEC_ID.manufacturer;
    uint8_t type = flash_JEDEC_ID.memory_type;
    uint8_t cap = flash_JEDEC_ID.capacity;
    uint32_t size = (cap > 0 && cap <= 31) ? (1u << cap) : 0;

    printf("=== Flash JEDEC ID ===\r\n");
    printf("  Raw              : 0x%02X 0x%02X 0x%02X\r\n\r\n", mfr, type, cap);
    printf("  Manufacturer     : %s (0x%02X)\r\n", get_name_by_id(flash_manufacturer_map, mfr), mfr);
    printf("  Memory Type      : %s (0x%02X)\r\n", get_name_by_id(flash_memory_type_map, type), type);
    printf("  Flash Capacity   : %lu MB (%lu bytes)\r\n", size >> 20, size);
    printf("\r\n");
    printf("  --- Additional info ---\r\n");
    printf("  Voltage          : %s\r\n", flash_voltage(type));
    printf("  Sectors          : %lu x 4KB\r\n", size / 4096);
    printf("  Blocks (64K)     : %lu x 64KB\r\n", size / 65536);
    printf("  Pages            : %lu x 256B\r\n", size / 256);
    printf("\r\n");

    return 0;
}
