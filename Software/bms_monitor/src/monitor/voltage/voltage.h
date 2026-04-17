#ifndef VOLTAGE_H
#define VOLTAGE_H

#include <stdint.h>
#include <stdbool.h>

#define NUM_CELLS_PER_MONITOR 16U

typedef struct
{
    int16_t raw;
    float voltage;
    bool active;
} cell_voltage_t;

typedef struct
{
    cell_voltage_t cells[NUM_CELLS_PER_MONITOR];
} cell_voltage_data_t;

int voltage_init(void);
int read_cell_voltages(cell_voltage_data_t *voltages);

#endif