#include "uart_task.h"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include "hardware/gpio.h"
#include "hardware/uart.h"

#include "tusb.h"
#include "usb_itf.h"


#define UART0_TX_PIN         0
#define UART0_RX_PIN         1

#define UART1_TX_PIN         4
#define UART1_RX_PIN         5

#define BUFFER_SIZE         2560
#define DEFAULT_BAUDRATE    115200

extern int transfer_flag;

typedef struct {
    uint8_t itf;

    uint pin1;
    uint pin2;
    uart_inst_t *instance;
    uint32_t baudrate;

    cdc_line_coding_t uart;
    cdc_line_coding_t usb;

    SemaphoreHandle_t mtx;
    uint8_t buf_uart[BUFFER_SIZE];
    uint8_t buf_tmp[BUFFER_SIZE];
    size_t buf_uart_size;
    uint8_t buf_usb[BUFFER_SIZE];
} uart_config_t;

static uart_config_t configs[2] = {
    {
        .itf = ITF_UART0,

        .pin1 = UART0_TX_PIN,
        .pin2 = UART0_RX_PIN,
        .instance = uart0,
        .baudrate = DEFAULT_BAUDRATE,
        .uart = {0},
        .usb = {
            .bit_rate = DEFAULT_BAUDRATE,
            .stop_bits = 1,
            .parity = 0,
            .data_bits = 8,
        },

        .buf_uart = {0},
        .buf_tmp = {0},
        .buf_uart_size = 0,
        .buf_usb = {0},
    },
    {
        .itf = ITF_UART1,

        .pin1 = UART1_TX_PIN,
        .pin2 = UART1_RX_PIN,
        .instance = uart1,
        .baudrate = DEFAULT_BAUDRATE,
        .uart = {0},
        .usb = {
            .bit_rate = DEFAULT_BAUDRATE,
            .stop_bits = 1,
            .parity = 0,
            .data_bits = 8,
        },

        .buf_uart = {0},
        .buf_tmp = {0},
        .buf_uart_size = 0,
        .buf_usb = {0},
    },
};


void uart0_irq() {
    xSemaphoreTakeFromISR(configs[0].mtx, NULL);
    while (uart_is_readable(configs[0].instance) && configs[0].buf_uart_size < sizeof(configs[0].buf_uart)) {
        configs[0].buf_uart[configs[0].buf_uart_size++] = uart_getc(configs[0].instance);
    }
    xSemaphoreGiveFromISR(configs[0].mtx, NULL);
}

void uart1_irq() {
    xSemaphoreTakeFromISR(configs[1].mtx, NULL);
    while (uart_is_readable(configs[1].instance) && configs[1].buf_uart_size < sizeof(configs[1].buf_uart)) {
        configs[1].buf_uart[configs[1].buf_uart_size++] = uart_getc(configs[1].instance);
    }
    xSemaphoreGiveFromISR(configs[1].mtx, NULL);
}

static const uart_parity_t parity_map[] = {
    [0] = UART_PARITY_NONE,
    [1] = UART_PARITY_ODD,
    [2] = UART_PARITY_EVEN,
    [3] = UART_PARITY_NONE,
};

static void configure_uart_bridge(uart_inst_t *instance, cdc_line_coding_t *usb, cdc_line_coding_t *uart) {
    if (usb->bit_rate != uart->bit_rate) {
        uart->bit_rate = usb->bit_rate;
        uart_set_baudrate(instance, uart->bit_rate);
    }

    if (
        usb->data_bits != uart->data_bits
        || usb->stop_bits != uart->stop_bits
        || usb->parity != uart->parity
    ) {
        uart->data_bits = usb->data_bits;
        uart->stop_bits = usb->stop_bits;
        uart->parity = usb->parity;

        auto parity = uart->parity < count_of(parity_map)
                          ? parity_map[uart->parity]
                          : parity_map[0];
        uart_set_format(
            instance,
            uart->data_bits,
            uart->stop_bits == 2 ? 2 : 1,
            parity
        );
    }
}

static void init_uart_bridge(uart_config_t &config) {
    // config init
    config.mtx = xSemaphoreCreateMutex();
    config.uart = {.bit_rate = DEFAULT_BAUDRATE};

    // hw init
    gpio_init(config.pin1);
    gpio_init(config.pin2);
    gpio_set_function(config.pin1, GPIO_FUNC_UART);
    gpio_set_function(config.pin2, GPIO_FUNC_UART);

    uart_init(config.instance, config.baudrate);
    uart_set_hw_flow(config.instance, false, false);
    configure_uart_bridge(config.instance, &config.usb, &config.uart);
    uart_set_fifo_enabled(config.instance, false);

    if (config.instance == uart1) {
        irq_set_exclusive_handler(UART1_IRQ, uart1_irq);
        irq_set_enabled(UART1_IRQ, true);
        uart_set_irq_enables(uart1, true, false);
    } else {
        irq_set_exclusive_handler(UART0_IRQ, uart0_irq);
        irq_set_enabled(UART0_IRQ, true);
        uart_set_irq_enables(uart0, true, false);
    }
}

void vTaskUart(__unused void *pvParams) {
    assert(pvParams != nullptr);
    auto *puUartNumber = static_cast<unsigned int *>(pvParams);
    const unsigned int uUartNumber = *puUartNumber;
    assert(uUartNumber < count_of(configs));

    init_uart_bridge(configs[uUartNumber]);

    auto &config = configs[uUartNumber];

    for (;;) {
        uint32_t len;

        // config
        tud_cdc_n_get_line_coding(config.itf, &config.usb);
        configure_uart_bridge(config.instance, &config.usb, &config.uart);

        // uart -> usb
        if (config.buf_uart_size > 0) {
            transfer_flag = 1;

            xSemaphoreTake(config.mtx, pdMS_TO_TICKS(100));
            len = config.buf_uart_size;
            memcpy(config.buf_tmp, config.buf_uart, len);
            config.buf_uart_size = 0;
            xSemaphoreGive(config.mtx);

            tud_cdc_n_write(config.itf, config.buf_tmp, len);
            tud_cdc_n_write_flush(config.itf);
        }

        // usb -> uart
        len = tud_cdc_n_available(config.itf);
        if (!len) {
            vTaskDelay(1);
            continue;
        }

        len = tud_cdc_n_read(config.itf, config.buf_usb, sizeof(config.buf_usb));

        if (len > 0) {
            transfer_flag = 1;
            uart_write_blocking(config.instance, config.buf_usb, len);
        }
    }
}
