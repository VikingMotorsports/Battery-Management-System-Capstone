#include "init.h"
#include "voltage.h"
#include "temperature.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
    int ret;
    cell_voltage_data_t voltages;
    temperature_data_t temperatures;

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

    ret = temperature_init();
    if (ret < 0)
    {
        LOG_ERR("temperature_init failed: %d", ret);
        return 0;
    }

    LOG_INF("temperature init complete");

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

        ret = read_temperatures(&temperatures);
        if (ret < 0)
        {
            LOG_ERR("read_temperatures failed: %d", ret);
        }
        else
        {
            for (int i = 0; i < NUM_THERMISTORS_PER_MONITOR; i++)
            {
                if (!temperatures.thermistors[i].active)
                {
                    printk("Thermistor %d: inactive\n", i + 1);
                    continue;
                }

                float temp_c = temperatures.thermistors[i].temperature_c;
                int temp_whole = (int)temp_c;
                int temp_frac = (int)((temp_c - (float)temp_whole) * 1000.0f);

                if (temp_frac < 0)
                {
                    temp_frac = -temp_frac;
                }

                float gpio_v = temperatures.thermistors[i].gpio_volts;
                int gpio_whole = (int)gpio_v;
                int gpio_frac = (int)((gpio_v - (float)gpio_whole) * 1000000.0f);

                if (gpio_frac < 0)
                {
                    gpio_frac = -gpio_frac;
                }

                float tsref_v = temperatures.thermistors[i].tsref_volts;
                int tsref_whole = (int)tsref_v;
                int tsref_frac = (int)((tsref_v - (float)tsref_whole) * 1000000.0f);

                if (tsref_frac < 0)
                {
                    tsref_frac = -tsref_frac;
                }

                float rntc = temperatures.thermistors[i].thermistor_ohms;
                int r_whole = (int)rntc;
                int r_frac = (int)((rntc - (float)r_whole) * 10.0f);

                if (r_frac < 0)
                {
                    r_frac = -r_frac;
                }

                printk("Thermistor %d: raw_gpio=0x%04X raw_tsref=0x%04X "
                       "gpio=%d.%06d V tsref=%d.%06d V Rntc=%d.%1d ohm T=%d.%03d C\n",
                       i + 1,
                       temperatures.thermistors[i].raw_gpio,
                       temperatures.thermistors[i].raw_tsref,
                       gpio_whole,
                       gpio_frac,
                       tsref_whole,
                       tsref_frac,
                       r_whole,
                       r_frac,
                       temp_whole,
                       temp_frac);
            }
        }

        printk("\n");
        k_msleep(1000);
    }

    return 0;
}