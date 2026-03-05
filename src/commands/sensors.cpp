#include <stdio.h>
#include "hardware/adc.h"

int command_sensors(int, const char *[]) {
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);

    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    float adc = (float) adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    printf("Onboard temperature: %+.01f °C\n", tempC);

    return 0;
}
