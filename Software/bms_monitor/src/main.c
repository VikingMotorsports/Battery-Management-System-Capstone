#include "init.h"
#include "voltage.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
    int ret;
    cell_voltage_data_t voltages;

    LOG_INF("battery monitor startup");

    ret = monitor_init();
    if (ret < 0)
    {
        LOG_ERR("monitor_init failed: %d", ret);
        return 0;
    }

    LOG_INF("monitor init complete");

    ret = voltage_init();
    if (ret < 0)
    {
        LOG_ERR("voltage_init failed: %d", ret);
        return 0;
    }

    LOG_INF("voltage init complete");

    while (1)
    {
        ret = read_cell_voltages(&voltages);
        if (ret < 0)
        {
            LOG_ERR("read_cell_voltages failed: %d", ret);
            k_msleep(1000);
            continue;
        }

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
        k_msleep(1000);
    }

    return 0;
}