#ifndef __ONBOARD_TEMPERATURE_H__
#define __ONBOARD_TEMPERATURE_H__

/* Choose 'C' for Celsius or 'F' for Fahrenheit. */
#define TEMPERATURE_UNITS 'C'

void onboard_temperature_init(void);

/** References for this implementation:
 * raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
 * pico-examples/adc/adc_console/adc_console.c */
float onboard_temperature_read(const char unit);

#endif /* __ONBOARD_TEMPERATURE_H__ */
