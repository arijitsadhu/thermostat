#include "onboard_temperature.h"
#include "hardware/adc.h"
#include "pico/stdlib.h"

void onboard_temperature_init(void)
{
    /* Initialize hardware AD converter, enable onboard temperature sensor and
     *   select its channel (do this once for efficiency, but beware that this
     *   is a global operation). */
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
}

/* References for this implementation:
 * raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
 * pico-examples/adc/adc_console/adc_console.c */
float onboard_temperature_read(const char unit)
{

    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    if (unit == 'C') {
        return tempC;
    } else if (unit == 'F') {
        return tempC * 9 / 5 + 32;
    }

    return -1.0f;
}
