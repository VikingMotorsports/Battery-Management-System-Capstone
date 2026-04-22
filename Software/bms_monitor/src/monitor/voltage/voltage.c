#include "voltage.h"
#include "protocol.h"
#include "../register_maps/bq79616_regs.h"

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(voltage, LOG_LEVEL_INF);

#define STACK_ADDR_UNUSED 0U
#define ACTIVE_CELL_6 0x00U
#define ADC_CTRL1_CONTINUOUS_MAIN_GO 0x06U

#define VCELL_BLOCK_NUM_BYTES 32U
#define VCELL_RESPONSE_FRAME_LEN (VCELL_BLOCK_NUM_BYTES + 6U)
#define VCELL_UNUSED_RAW ((int16_t)0x8000)

static void parse_cell_voltages(const uint8_t *rx_buf, cell_voltage_data_t *voltages);

int voltage_init(void)
{
    int ret;
    uint8_t data;

    data = ACTIVE_CELL_6;
    ret = write_reg(STACK_WRITE, STACK_ADDR_UNUSED, BQ79616_REG_ACTIVE_CELL, &data, 1U);
    if (ret < 0)
    {
        LOG_ERR("ACTIVE_CELL write failed: reg=0x%04X err=%d", BQ79616_REG_ACTIVE_CELL, ret);
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

int read_cell_voltages(cell_voltage_data_t *voltages)
{
    uint8_t rx_buf[VCELL_RESPONSE_FRAME_LEN];
    int ret;

    if (voltages == NULL)
    {
        LOG_ERR("read_cell_voltages got NULL output pointer");
        return -EINVAL;
    }

    k_usleep(500);

    ret = read_reg(STACK_READ, STACK_ADDR_UNUSED, BQ79616_REG_VCELL16_HI, rx_buf, sizeof(rx_buf), VCELL_BLOCK_NUM_BYTES);
    if (ret < 0)
    {
        LOG_ERR("VCELL block read failed: reg=0x%04X err=%d", BQ79616_REG_VCELL16_HI, ret);
        return ret;
    }

    parse_cell_voltages(rx_buf, voltages);
    return 0;
}

static void parse_cell_voltages(const uint8_t *rx_buf, cell_voltage_data_t *voltages)
{
    const uint8_t *payload = &rx_buf[4];

    for (size_t i = 0; i < NUM_CELLS_PER_MONITOR; i++)
    {
        size_t payload_index = i * 2U;
        size_t cell_index = 15U - i;

        int16_t raw = (int16_t)(((uint16_t)payload[payload_index] << 8) | (uint16_t)payload[payload_index + 1U]);

        voltages->cells[cell_index].raw = raw;

        if (raw == VCELL_UNUSED_RAW)
        {
            voltages->cells[cell_index].active = false;
            voltages->cells[cell_index].voltage = 0.0f;
        }
        else
        {
            voltages->cells[cell_index].active = true;
            voltages->cells[cell_index].voltage = ((float)raw * 190.73f) / 1000000.0f;
        }
    }
}