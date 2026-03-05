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

static uart_config_t config0 = {
    .itf = ITF_UART0,

    .pin1 = UART0_TX_PIN,
    .pin2 = UART0_RX_PIN,
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
};


void uart_irq() {
    xSemaphoreTakeFromISR(config0.mtx, NULL);
    while (uart_is_readable(config0.instance) && config0.buf_uart_size < sizeof(config0.buf_uart)) {
        config0.buf_uart[config0.buf_uart_size++] = uart_getc(config0.instance);
    }
    xSemaphoreGiveFromISR(config0.mtx, NULL);
}

static uart_inst_t *detect_instance(uint pin1, uint pin2) {
    static const int8_t gpio_uart_map[] = {
        0, 0,
        -1, -1,
        1, 1,
        -1, -1,
        1, 1,
        -1, -1,
        0, 0,
        -1, -1,
        0, 0,
    };

    assert(pin1 < count_of(gpio_uart_map));
    assert(pin2 < count_of(gpio_uart_map));
    assert(gpio_uart_map[pin1] == gpio_uart_map[pin2]);

    if (gpio_uart_map[pin1] == 1) {
        return uart1;
    }

    return uart0;
}

static void configure_uart_bridge(uart_inst_t *instance, cdc_line_coding_t *usb, cdc_line_coding_t *uart) {
    if (usb->bit_rate != uart->bit_rate) {
        uart->bit_rate = usb->bit_rate;
        uart_set_baudrate(config0.instance, uart->bit_rate);
    }

    if (
        usb->data_bits != uart->data_bits
        || usb->stop_bits != uart->stop_bits
        || usb->parity != uart->parity
    ) {
        uart->data_bits = usb->data_bits;
        uart->stop_bits = usb->stop_bits;
        uart->parity = usb->parity;


        uart_set_format(
            instance,
            uart->data_bits,
            uart->stop_bits == 2 ? 2 : 1,
            static_cast<uart_parity_t>(0)
        );
    }
}

void vTaskUart(__unused void *pvParams) {
    // config init
    config0.instance = detect_instance(config0.pin1, config0.pin2);
    config0.mtx = xSemaphoreCreateMutex();
    config0.uart = {.bit_rate = DEFAULT_BAUDRATE};

    // hw init
    gpio_init(config0.pin1);
    gpio_init(config0.pin2);
    gpio_set_function(config0.pin1, GPIO_FUNC_UART);
    gpio_set_function(config0.pin2, GPIO_FUNC_UART);

    uart_init(config0.instance, config0.baudrate);
    uart_set_hw_flow(config0.instance, false, false);
    configure_uart_bridge(config0.instance, &config0.usb, &config0.uart);
    uart_set_fifo_enabled(config0.instance, false);

    irq_set_exclusive_handler(UART0_IRQ, uart_irq);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(config0.instance, true, false);


    for (;;) {
        uint32_t len;

        // config
        tud_cdc_n_get_line_coding(config0.itf, &config0.usb);
        configure_uart_bridge(config0.instance, &config0.usb, &config0.uart);

        // uart -> usb
        if (config0.buf_uart_size > 0) {
            transfer_flag = 1;

            xSemaphoreTake(config0.mtx, pdMS_TO_TICKS(100));
            len = config0.buf_uart_size;
            memcpy(config0.buf_tmp, config0.buf_uart, len);
            config0.buf_uart_size = 0;
            xSemaphoreGive(config0.mtx);

            tud_cdc_n_write(config0.itf, config0.buf_tmp, len);
            tud_cdc_n_write_flush(config0.itf);
        }

        // usb -> uart
        len = tud_cdc_n_available(config0.itf);
        if (!len) {
            vTaskDelay(1);
            continue;
        }

        len = tud_cdc_n_read(config0.itf, config0.buf_usb, sizeof(config0.buf_usb));

        if (len > 0) {
            transfer_flag = 1;
            uart_write_blocking(config0.instance, config0.buf_usb, len);
        }
    }
}
