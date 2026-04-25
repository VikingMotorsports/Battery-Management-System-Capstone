#include "temperature.h"
#include "protocol.h"
#include "../register_maps/bq79616_regs.h"

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(temperature, LOG_LEVEL_INF);

#define STACK_ADDR_UNUSED 0U
#define CONTROL2_TSREF_EN 0x01U
#define ADC_CTRL1_CONTINUOUS_MAIN_GO 0x06U

#define GPIO_MODE_ADC_ONLY 0x12U

#define TEMP_NUM_BYTES 18U
#define TEMP_RESPONSE_FRAME_LEN (TEMP_NUM_BYTES + 6U)
#define TEMP_UNUSED_RAW 0x8000U

#define GPIO_LSB_UV 152.59f
#define TSREF_LSB_UV 169.54f

#define NTC_PULLUP_OHMS 10000.0f
#define NTC_R25_OHMS 10000.0f
#define NTC_BETA_K 3380.0f
#define T0_KELVIN 298.15f

static void parse_temperatures(const uint8_t *rx_buf, temperature_data_t *temperatures);

int temperature_init(void)
{
    int ret;
    uint8_t data;
    static const uint8_t all_gpio_thermistors_adc_only[4] = {GPIO_MODE_ADC_ONLY, 0x00, 0x00, 0x00};

    data = CONTROL2_TSREF_EN;
    ret = write_reg(STACK_WRITE, STACK_ADDR_UNUSED, BQ79616_REG_CONTROL2, &data, 1U);
    if (ret < 0)
    {
        LOG_ERR("CONTROL2 write failed: reg=0x%04X err=%d", BQ79616_REG_CONTROL2, ret);
        return ret;
    }

    k_msleep(2);

    ret = write_reg(STACK_WRITE, STACK_ADDR_UNUSED, BQ79616_REG_GPIO_CONF1, all_gpio_thermistors_adc_only, 4U);
    if (ret < 0)
    {
        LOG_ERR("GPIO_CONF1-4 write failed: reg=0x%04X err=%d", BQ79616_REG_GPIO_CONF1, ret);
        return ret;
    }

    data = ADC_CTRL1_CONTINUOUS_MAIN_GO;
    ret = write_reg(STACK_WRITE, STACK_ADDR_UNUSED, BQ79616_REG_ADC_CTRL1, &data, 1U);
    if (ret < 0)
    {
        LOG_ERR("ADC_CTRL1 write failed: reg=0x%04X err=%d", BQ79616_REG_ADC_CTRL1, ret);
        return ret;
    }

    return 0;
}

int read_temperatures(temperature_data_t *temperatures)
{
    uint8_t rx_buf[TEMP_RESPONSE_FRAME_LEN];
    int ret;

    if (temperatures == NULL)
    {
        LOG_ERR("read_temperatures got NULL output pointer");
        return -EINVAL;
    }

    k_msleep(2);

    ret = read_reg(STACK_READ, STACK_ADDR_UNUSED, BQ79616_REG_TSREF_HI, rx_buf, sizeof(rx_buf), TEMP_NUM_BYTES);
    if (ret < 0)
    {
        LOG_ERR("temperature block read failed: reg=0x%04X err=%d", BQ79616_REG_TSREF_HI, ret);
        return ret;
    }

    parse_temperatures(rx_buf, temperatures);
    return 0;
}

static void parse_temperatures(const uint8_t *rx_buf, temperature_data_t *temperatures)
{
    const uint8_t *payload = &rx_buf[4];

    uint16_t raw_tsref = (uint16_t)(((uint16_t)payload[0] << 8) | (uint16_t)payload[1]);

    for (size_t i = 0; i < NUM_THERMISTORS_PER_MONITOR; i++)
    {
        size_t payload_index = 2U + (i * 2U);
        uint16_t raw_gpio = (uint16_t)(((uint16_t)payload[payload_index] << 8) | (uint16_t)payload[payload_index + 1U]);

        temperatures->thermistors[i].raw_tsref = raw_tsref;
        temperatures->thermistors[i].raw_gpio = raw_gpio;

        if ((raw_tsref == TEMP_UNUSED_RAW) || (raw_gpio == TEMP_UNUSED_RAW))
        {
            temperatures->thermistors[i].active = false;
            temperatures->thermistors[i].tsref_volts = 0.0f;
            temperatures->thermistors[i].gpio_volts = 0.0f;
            temperatures->thermistors[i].thermistor_ohms = 0.0f;
            temperatures->thermistors[i].temperature_c = 0.0f;
        }
        else
        {
            temperatures->thermistors[i].active = true;

            temperatures->thermistors[i].tsref_volts = ((float)raw_tsref * TSREF_LSB_UV) / 1000000.0f;

            temperatures->thermistors[i].gpio_volts = ((float)raw_gpio * GPIO_LSB_UV) / 1000000.0f;

            temperatures->thermistors[i].thermistor_ohms = NTC_PULLUP_OHMS *
                                                           (temperatures->thermistors[i].gpio_volts /
                                                            (temperatures->thermistors[i].tsref_volts - temperatures->thermistors[i].gpio_volts));

            float temp_k = (T0_KELVIN * NTC_BETA_K) / (T0_KELVIN * logf(temperatures->thermistors[i].thermistor_ohms / NTC_R25_OHMS) + NTC_BETA_K);
            float temp_c = temp_k - 273.15f;

            temperatures->thermistors[i].temperature_c = temp_c;
        }
    }
}