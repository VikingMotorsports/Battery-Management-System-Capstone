#include "init.h"
#include "voltage.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
    int ret;
    cell_voltage_data_t voltages;

    ret = monitor_init();
    if (ret < 0)
    {
        return 0;
    }

    ret = voltage_init();
    if (ret < 0)
    {
        return 0;
    }

    while (1)
    {
        ret = read_cell_voltages(&voltages);
        if (ret == 0)
        {
            for (int i = 0; i < NUM_CELLS_PER_MONITOR; i++)
            {
                if (!voltages.cells[i].active)
                {
                    printk("Cell %2d: inactive\n", i + 1);
                    continue;
                }

                float volts = voltages.cells[i].voltage;
                int whole = (int)volts;
                int frac = (int)((volts - (float)whole) * 1000000.0f);

                if (frac < 0)
                {
                    frac = -frac;
                }

                printk("Cell %2d: raw=0x%04X voltage=%d.%06d V\n",
                       i + 1,
                       (uint16_t)voltages.cells[i].raw,
                       whole,
                       frac);
            }

            printk("\n");
        }

        k_msleep(1000);
    }

    return 0;
}