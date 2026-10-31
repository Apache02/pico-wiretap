#include "uart_task.h"
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/uart.h"

#include "tusb.h"
#include "usb_itf.h"
#include "utils/ring_buffer.h"


#define UART0_TX_PIN         0
#define UART0_RX_PIN         1

#define UART1_TX_PIN         4
#define UART1_RX_PIN         5

// UART -> USB backlog, the only place bytes wait while the host is slow to read:
// ~180 ms at 921600 baud, ~1.4 s at 115200.
#define RX_BUFFER_SIZE      16384
#define DEFAULT_BAUDRATE    115200

// FIFO trigger levels, in eighths of the 32-entry FIFOs (0 = 1/8 ... 4 = 7/8). Half-full
// leaves the IRQ 16 bytes of slack before an overrun and the task 16 bytes before the
// TX line goes idle.
#define RX_FIFO_LEVEL       2
#define TX_FIFO_LEVEL       2

// How long the task sleeps with no IRQ or USB callback to wake it; a safety net only.
#define IDLE_TIMEOUT        pdMS_TO_TICKS(10)

extern volatile int transfer_flag;

RINGBUF_DECLARE(uart0_rx, RX_BUFFER_SIZE);
RINGBUF_DECLARE(uart1_rx, RX_BUFFER_SIZE);

typedef struct {
    uint8_t itf;

    uint tx_pin;
    uint rx_pin;
    uart_inst_t *instance;
    uint irq;

    // Woken by the UART IRQ and by the CDC callbacks.
    TaskHandle_t volatile task;

    // What the UART is currently configured to.
    cdc_line_coding_t line_coding;

    // UART -> USB: filled by the IRQ, drained by the task.
    struct ringbuf *rx;

    // USB -> UART: read from CDC, not yet in the UART FIFO. Bytes the UART cannot take stay
    // in the CDC FIFO, so TinyUSB NAKs the host instead of anything being dropped.
    uint8_t tx[CFG_TUD_CDC_RX_BUFSIZE];
    size_t tx_len;
    size_t tx_pos;

    // Loss counters, never reset.
    volatile uint32_t overruns;  // UART FIFO overflowed before the IRQ emptied it
    volatile uint32_t dropped;   // rx was full, the byte was discarded
    volatile uint32_t errors;    // received with a framing, parity or break error
} uart_bridge_t;

static uart_bridge_t bridges[2] = {
    {
        .itf = ITF_UART0,
        .tx_pin = UART0_TX_PIN,
        .rx_pin = UART0_RX_PIN,
        .instance = uart0,
        .irq = UART0_IRQ,
        .rx = &uart0_rx,
    },
    {
        .itf = ITF_UART1,
        .tx_pin = UART1_TX_PIN,
        .rx_pin = UART1_RX_PIN,
        .instance = uart1,
        .irq = UART1_IRQ,
        .rx = &uart1_rx,
    },
};


static void notify_bridge(uart_bridge_t &bridge) {
    TaskHandle_t task = bridge.task;
    if (task) xTaskNotifyGive(task);
}

static void uart_bridge_irq(uart_bridge_t &bridge) {
    uart_hw_t *hw = uart_get_hw(bridge.instance);

    // Empty the FIFO even when rx is full: a byte left behind keeps the IRQ asserted and
    // would lock this core inside the handler.
    while (!(hw->fr & UART_UARTFR_RXFE_BITS)) {
        const uint32_t dr = hw->dr;

        if (dr & UART_UARTDR_OE_BITS) bridge.overruns++;
        if (dr & (UART_UARTDR_BE_BITS | UART_UARTDR_PE_BITS | UART_UARTDR_FE_BITS)) bridge.errors++;

        if (!ringbuf_put_byte(bridge.rx, (uint8_t) dr)) bridge.dropped++;
    }

    // The TX FIFO has room again; the task re-arms this once it fills the FIFO up.
    if (hw->mis & UART_UARTMIS_TXMIS_BITS) hw_clear_bits(&hw->imsc, UART_UARTIMSC_TXIM_BITS);

    BaseType_t woken = pdFALSE;
    if (bridge.task) vTaskNotifyGiveFromISR(bridge.task, &woken);
    portYIELD_FROM_ISR(woken);
}

static void uart0_irq() { uart_bridge_irq(bridges[0]); }
static void uart1_irq() { uart_bridge_irq(bridges[1]); }

static uart_bridge_t *find_bridge(uint8_t itf) {
    for (auto &bridge: bridges) {
        if (bridge.itf == itf) return &bridge;
    }
    return nullptr;
}

// TinyUSB callbacks, called from tud_task(). The shell and plot interfaces poll instead.
extern "C" {
    void tud_cdc_rx_cb(uint8_t itf) {
        if (auto *bridge = find_bridge(itf)) notify_bridge(*bridge);
    }

    void tud_cdc_tx_complete_cb(uint8_t itf) {
        if (auto *bridge = find_bridge(itf)) notify_bridge(*bridge);
    }

    void tud_cdc_line_state_cb(uint8_t itf, __unused bool dtr, __unused bool rts) {
        if (auto *bridge = find_bridge(itf)) notify_bridge(*bridge);
    }
}

// Indexed by the CDC parity code; mark (3) and space (4) have no UART equivalent.
static const uart_parity_t parity_map[] = {
    [0] = UART_PARITY_NONE,
    [1] = UART_PARITY_ODD,
    [2] = UART_PARITY_EVEN,
};

static void apply_line_coding(uart_bridge_t &bridge) {
    cdc_line_coding_t lc;
    tud_cdc_n_get_line_coding(bridge.itf, &lc);

    // Hosts may send 0 while closing the port; there is nothing sensible to apply.
    if (lc.bit_rate == 0) return;

    // Clamp what the UART cannot do, so a bad request neither asserts nor gets reapplied.
    if (lc.data_bits < 5 || lc.data_bits > 8) lc.data_bits = 8;
    if (lc.stop_bits != 2) lc.stop_bits = 0;   // 1.5 stop bits becomes 1
    if (lc.parity >= count_of(parity_map)) lc.parity = 0;

    if (memcmp(&lc, &bridge.line_coding, sizeof(lc)) == 0) return;

    // Bytes already handed to the UART leave at the rate they were sent for.
    uart_tx_wait_blocking(bridge.instance);

    if (lc.bit_rate != bridge.line_coding.bit_rate) {
        uart_set_baudrate(bridge.instance, lc.bit_rate);
    }
    uart_set_format(bridge.instance, lc.data_bits, lc.stop_bits == 2 ? 2 : 1, parity_map[lc.parity]);

    bridge.line_coding = lc;
}

static void uart_to_usb(uart_bridge_t &bridge) {
    // Nobody has the port open, so nobody would read this; keeping it would bury the live
    // stream under stale bytes once someone does.
    if (!tud_cdc_n_connected(bridge.itf)) {
        ringbuf_skip(bridge.rx, ringbuf_count(bridge.rx));
        return;
    }

    bool moved = false;
    for (;;) {
        const uint8_t *data;
        size_t len = ringbuf_peek(bridge.rx, &data);
        if (!len) break;

        // Takes what fits; the rest waits in rx until tud_cdc_tx_complete_cb.
        len = tud_cdc_n_write(bridge.itf, data, len);
        if (!len) break;

        ringbuf_skip(bridge.rx, len);
        moved = true;
    }

    if (moved) {
        transfer_flag = 1;
        tud_cdc_n_write_flush(bridge.itf);
    }
}

static void usb_to_uart(uart_bridge_t &bridge) {
    uart_hw_t *hw = uart_get_hw(bridge.instance);

    for (;;) {
        if (bridge.tx_pos == bridge.tx_len) {
            bridge.tx_pos = 0;
            bridge.tx_len = tud_cdc_n_read(bridge.itf, bridge.tx, sizeof(bridge.tx));
            if (!bridge.tx_len) return;

            transfer_flag = 1;
        }

        while (bridge.tx_pos < bridge.tx_len && !(hw->fr & UART_UARTFR_TXFF_BITS)) {
            hw->dr = bridge.tx[bridge.tx_pos++];
        }

        if (bridge.tx_pos < bridge.tx_len) {
            // FIFO is full, so above its trigger level: the IRQ fires once it drains below.
            hw_set_bits(&hw->imsc, UART_UARTIMSC_TXIM_BITS);
            return;
        }
    }
}

static void init_uart_bridge(uart_bridge_t &bridge) {
    gpio_set_function(bridge.tx_pin, GPIO_FUNC_UART);
    gpio_set_function(bridge.rx_pin, GPIO_FUNC_UART);

    uart_init(bridge.instance, DEFAULT_BAUDRATE);
    uart_set_hw_flow(bridge.instance, false, false);
    uart_set_fifo_enabled(bridge.instance, true);
    apply_line_coding(bridge);

    uart_hw_t *hw = uart_get_hw(bridge.instance);
    hw_write_masked(
        &hw->ifls,
        (RX_FIFO_LEVEL << UART_UARTIFLS_RXIFLSEL_LSB) | (TX_FIFO_LEVEL << UART_UARTIFLS_TXIFLSEL_LSB),
        UART_UARTIFLS_RXIFLSEL_BITS | UART_UARTIFLS_TXIFLSEL_BITS
    );

    // The task must be known before the first IRQ can try to wake it.
    bridge.task = xTaskGetCurrentTaskHandle();

    // Enabled from the task, so the IRQ lands on the task's core. The IRQ and the task are
    // then the producer and consumer of rx on one core, which ring_buffer.c relies on.
    irq_set_exclusive_handler(bridge.irq, bridge.instance == uart0 ? uart0_irq : uart1_irq);
    irq_set_enabled(bridge.irq, true);

    // RX on the FIFO level, plus RX timeout for the tail of a burst below that level.
    hw->imsc = UART_UARTIMSC_RXIM_BITS | UART_UARTIMSC_RTIM_BITS;
}

void vTaskUart(void *pvParams) {
    const auto uUartNumber = reinterpret_cast<uintptr_t>(pvParams);
    assert(uUartNumber < count_of(bridges));

    auto &bridge = bridges[uUartNumber];
    init_uart_bridge(bridge);

    for (;;) {
        apply_line_coding(bridge);
        uart_to_usb(bridge);
        usb_to_uart(bridge);

        ulTaskNotifyTake(pdTRUE, IDLE_TIMEOUT);
    }
}
