#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <stdbool.h>
#include <stdint.h>

#define NUM_THERMISTORS_PER_MONITOR 8U

typedef struct
{
    uint16_t raw_gpio;
    uint16_t raw_tsref;
    float gpio_volts;
    float tsref_volts;
    float thermistor_ohms;
    float temperature_c;
    bool active;
} thermistor_data_t;

typedef struct
{
    thermistor_data_t thermistors[NUM_THERMISTORS_PER_MONITOR];
} temperature_data_t;

int temperature_init(void);
int read_temperatures(temperature_data_t *temperatures);

#endif