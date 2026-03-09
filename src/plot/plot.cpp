#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include <string.h>
#include "usb_itf.h"

#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "cdc_printf.h"
#include "shell/Parser.h"
#include "shell/console_colors.h"

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------

#define PLOT_DEFAULT_RATE   1   // samples per second
#define PLOT_RATE_MIN       1
#define PLOT_RATE_MAX       200

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------

struct Measurement {
    bool enabled;

    union {
        uint32_t value;
        float voltage;

        struct {
            uint32_t frequency;
            uint8_t duty;
        } pwm;
    };
};

static struct {
    Measurement gpio14;
    Measurement gpio15;
    Measurement pwm6;
    Measurement pwm7;
    Measurement pwm12;
    Measurement pwm13;
    Measurement adc26;
    Measurement adc27;
    Measurement adc28;
    Measurement adc29;
    uint32_t rate; // samples per second
} state = {
    .gpio14 = {false},
    .gpio15 = {false},
    .pwm6 = {false},
    .pwm7 = {false},
    .pwm12 = {false},
    .pwm13 = {false},
    .adc26 = {false},
    .adc27 = {false},
    .adc28 = {false},
    .adc29 = {false},
    .rate = PLOT_DEFAULT_RATE,
};

typedef enum {
    GPIO = 0,
    ADC,
    PWM,
} plot_mode_t;

static const struct {
    uint pin;
    plot_mode_t mode;
    Measurement *ptr;
} measurements[] = {
    {14, GPIO, &state.gpio14},
    {15, GPIO, &state.gpio15},
    {26, ADC, &state.adc26},
    {27, ADC, &state.adc27},
    {28, ADC, &state.adc28},
    {29, ADC, &state.adc29},
    {6, PWM, &state.pwm6},
    {7, PWM, &state.pwm7},
    {12, PWM, &state.pwm12},
    {13, PWM, &state.pwm13},
};

static constexpr struct {
    plot_mode_t mode;
    const char *const name;
} mode_names[] = {
    {GPIO, "gpio"},
    {ADC, "adc"},
    {PWM, "pwm"},
};

// ---------------------------------------------------------------------------
// Mode helpers
// ---------------------------------------------------------------------------

static int get_pins_by_mode(uint *pins, plot_mode_t mode, int max) {
    int n = 0;
    for (auto m: measurements) {
        if (m.mode == mode && n < max) {
            pins[n++] = m.pin;
        }
    }
    return n;
}

static bool find_mode_by_name(const char *name, plot_mode_t *mode) {
    for (auto m: mode_names) {
        if (strcmp(m.name, name) == 0) {
            *mode = m.mode;
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// Task handle
// ---------------------------------------------------------------------------

TaskHandle_t xhTaskPlot = nullptr;

// ---------------------------------------------------------------------------
// Hardware read helpers
// ---------------------------------------------------------------------------

static uint32_t read_gpio(uint pin) {
    return gpio_get(pin) ? 1 : 0;
}

static float read_adc(uint8_t pin) {
    // ADC channel: pin 26 -> ch0, 27 -> ch1, 28 -> ch2
    adc_select_input(pin - 26u);
    uint16_t raw = adc_read();
    constexpr unsigned long MAX_U12 = (1 << 12) - 1;
    return static_cast<float>(raw) * (3.3f / static_cast<float>(MAX_U12)); // 12-bit, 3.3V ref
}

// PWM reading not yet implemented.
static bool read_pwm(uint8_t pin, uint32_t *out_freq, uint8_t *out_duty) {
    (void) pin;
    (void) out_freq;
    (void) out_duty;
    return false; // false = not implemented
}

// ---------------------------------------------------------------------------
// Measure block
// ---------------------------------------------------------------------------

static void measure_all() {
    for (auto m: measurements) {
        if (!m.ptr->enabled) continue;

        switch (m.mode) {
            case GPIO:
                m.ptr->value = read_gpio(m.pin);
                break;
            case PWM:
                read_pwm(m.pin, &m.ptr->pwm.frequency, &m.ptr->pwm.duty);
                break;
            case ADC:
                m.ptr->voltage = read_adc(m.pin);
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Print block
// ---------------------------------------------------------------------------

static void print_all() {
    for (auto m: measurements) {
        if (!m.ptr->enabled) continue;

        auto pin = m.pin;
        switch (m.mode) {
            case GPIO:
                cdc_printf(ITF_PLOT, "gpio%d:%u ", pin, m.ptr->value);
                break;
            case ADC:
                cdc_printf(ITF_PLOT, "adc%d:%.3f ", pin, m.ptr->voltage);
                break;
            case PWM:
                cdc_printf(ITF_PLOT, "pwm%d_f:%u pwm%d_d:%u ", pin, m.ptr->pwm.frequency, pin, m.ptr->pwm.duty);
                break;
        }
    }

    cdc_puts(ITF_PLOT, "\r\n");
}

// ---------------------------------------------------------------------------
// Plot task
// ---------------------------------------------------------------------------

static bool any_enabled() {
    for (auto m: measurements) {
        if (m.ptr->enabled) {
            return true;
        }
    }
    return false;
}

void vTaskPlot(__unused void *pvParams) {
    adc_init();

    for (auto m: measurements) {
        m.ptr->enabled = false;
        m.ptr->value = 0;
    }

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (any_enabled()) {
            measure_all();
            print_all();
            vTaskDelay(pdMS_TO_TICKS(configTICK_RATE_HZ / state.rate));
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

static void plot_task_wake() {
    if (xhTaskPlot != nullptr)
        xTaskNotifyGive(xhTaskPlot);
}

// ---------------------------------------------------------------------------
// Usage strings
// ---------------------------------------------------------------------------

static void usage() {
    printf("Usage:\r\n");
    printf("    plot add <gpio|adc|pwm> <pin> [... pin]\r\n");
    printf("    plot remove <pin> [... pins]\r\n");
    printf("    plot remove all\r\n");
    printf("    plot rate\r\n");
    printf("    plot rate <%d-%d>\r\n", PLOT_RATE_MIN, PLOT_RATE_MAX);
    printf("    plot status [pin] [... pins]\r\n");
    printf("    plot reset\r\n");
    printf("\r\n");
    printf("Available pins:\r\n");

    uint pins[32];
    int count;
    for (auto m: mode_names) {
        count = get_pins_by_mode(pins, m.mode, count_of(pins));
        if (count < 1) continue;

        printf("    %s: ", m.name);
        for (auto i = 0; i < count; i++) {
            printf("%d", pins[i]);
            if (i < count - 1) {
                printf(" ");
            }
        }

        printf("\r\n");
    }
}

// ---------------------------------------------------------------------------
// Actions handlers
// ---------------------------------------------------------------------------

static int action_add(int argc, const char *argv[]) {
    if (argc < 2) {
        printf(COLOR_RED("Expected: <gpio|adc|pwm> <pin> [... pins]") "\r\n");
        usage();
        return 1;
    }

    const char *mode_str = argv[0];

    plot_mode_t mode;
    if (!find_mode_by_name(mode_str, &mode)) {
        printf(COLOR_RED("Unknown type") "\r\n");
        return 1;
    }

    if (mode == PWM) {
        printf(COLOR_RED("PWM monitoring not yet implemented") "\r\n");
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        auto result = take_int(argv[i]);
        if (result.is_err()) {
            printf(COLOR_RED("Invalid pin: %s") "\r\n", argv[i]);
            continue;
        }
        auto pin = (uint) result.r;

        bool found = false;
        for (auto m: measurements) {
            if (m.pin != pin || m.mode != mode) continue;
            found = true;
            if (!m.ptr->enabled) {
                if (mode == GPIO) {
                    gpio_init(pin);
                    gpio_set_dir(pin, GPIO_IN);
                } else if (mode == ADC) {
                    gpio_init(pin);
                    adc_gpio_init(pin);
                }
                m.ptr->enabled = true;
                printf("  added %s pin #%u\r\n", mode_str, pin);
            }
            break;
        }
        if (!found) {
            printf(COLOR_RED("Pin #%u not available for %s") "\r\n", pin, mode_str);
        }
    }

    return 0;
}

static int action_status(int argc, const char *argv[]) {
    for (auto m: measurements) {
        if (argc > 0) {
            bool requested = false;
            for (int i = 0; i < argc; i++) {
                auto result = take_int(argv[i]);
                if (!result.is_err() && (uint) result.r == m.pin) {
                    requested = true;
                    break;
                }
            }
            if (!requested) continue;
        }

        const char *mode_str = "?";
        for (auto mn: mode_names) {
            if (mn.mode == m.mode) {
                mode_str = mn.name;
                break;
            }
        }

        if (m.ptr->enabled) {
            switch (m.mode) {
                case GPIO:
                    printf("  %s #%u: " COLOR_GREEN("on") " val=%d\r\n", mode_str, m.pin, m.ptr->value != 0);
                    break;
                case ADC:
                    printf("  %s #%u: " COLOR_GREEN("on") " val=%.3fV\r\n", mode_str, m.pin, m.ptr->voltage);
                    break;
                case PWM:
                    printf("  %s #%u: " COLOR_GREEN("on") " f=%luHz d=%u%%\r\n", mode_str, m.pin, m.ptr->pwm.frequency,
                           m.ptr->pwm.duty);
                    break;
            }
        } else {
            printf("  %s #%u: off\r\n", mode_str, m.pin);
        }
    }

    return 0;
}

static int action_remove(int argc, const char *argv[]) {
    for (int i = 0; i < argc; i++) {
        auto result = take_int(argv[i]);
        if (result.is_err()) continue;
        auto pin = result.r;
        for (auto m: measurements) {
            if (m.pin == pin && m.ptr->enabled) {
                m.ptr->enabled = false;
                printf("  removed at pin #%d\r\n", m.pin);
            }
        }
    }

    return 0;
}

static int action_reset() {
    int count = 0;
    for (auto m: measurements) {
        if (m.ptr->enabled) {
            m.ptr->enabled = false;
            printf("  removed at pin #%d\r\n", m.pin);
            count++;
        }
    }
    if (count == 0) {
        printf("  nothing to remove\r\n");
    }

    return 0;
}

static int action_rate(int argc, const char *argv[]) {
    if (argc < 1) {
        printf("  rate: %u samples/sec\r\n", static_cast<unsigned>(state.rate));
        return 0;
    }

    int new_rate = take_int(argv[0]).ok_or(0);
    if (new_rate < PLOT_RATE_MIN || new_rate > PLOT_RATE_MAX) {
        printf(COLOR_RED("Invalid rate") "\r\n");
        return 1;
    }

    state.rate = new_rate;
    return 0;
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

int command_plot(int argc, const char *argv[]) {
    // Examples
    // plot add <gpio|adc|pwm> <pin> [... pins]
    // plot remove <pin> [... pins]
    // plot remove all
    // plot rate [set_rate]
    // plot status [... pins]
    // plot reset

    const char *command = argv[0];
    if (argc < 2) {
        printf(COLOR_RED("Invalid arguments") "\r\n");
        usage();
        return 1;
    }

    const char *action = argv[1];

    if (strcmp(action, "help") == 0) {
        usage();
        return 0;
    }

    if (strcmp(action, "add") == 0) {
        auto ret = action_add(argc - 2, argv + 2);
        if (any_enabled()) {
            plot_task_wake();
        }
        return ret;
    }
    if (strcmp(action, "remove") == 0) {
        if (argc < 3) {
            printf(COLOR_RED("Invalid pins") "\r\n");
            usage();
            return 1;
        }
        if (strcmp(argv[2], "all") == 0) {
            return action_reset();
        }
        return action_remove(argc - 2, argv + 2);
    }
    if (strcmp(action, "status") == 0) {
        return action_status(argc - 2, argv + 2);
    }
    if (strcmp(action, "reset") == 0) {
        return action_reset();
    }
    if (strcmp(action, "rate") == 0) {
        return action_rate(argc - 2, argv + 2);
    }

    printf(COLOR_RED("Unknown action") "\r\n");
    usage();
    return 1;
}
