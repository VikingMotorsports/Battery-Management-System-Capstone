#include "init.h"
#include "protocol.h"
#include "uart.h"
#include "../register_maps/bq79600_regs.h"
#include "../register_maps/bq79616_regs.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <errno.h>
#include <stdint.h>

#define UART_NODE DT_NODELABEL(usart1)
#define GPIO_NODE DT_PATH(zephyr_user)

#define BQ_PRE_PULSE_IDLE_US 1000U
#define BQ_WAKE_PULSE_US 2750U
#define BQ_WAKE_TO_WAKE_DELAY_US 3500U

#define BRIDGE_ADDR 0x00U
#define NUM_STACK_DEVICES 1U
#define OTP_DUMMY_VALUE 0x00U
#define ONE_BYTE_READ_FRAME_LEN 7U

PINCTRL_DT_DEFINE(UART_NODE);
static const struct pinctrl_dev_config *uart_pinctrl = PINCTRL_DT_DEV_CONFIG_GET(UART_NODE);

static const struct gpio_dt_spec gpio = GPIO_DT_SPEC_GET(GPIO_NODE, gpios);

static void line_drive_high(void);
static void line_drive_low(void);
static void send_single_wake_ping(void);
static void wake_bridge(void);
static int wake_stack(void);
static int auto_address(void);

int monitor_init(void)
{
    int ret;

    if (!gpio_is_ready_dt(&gpio))
    {
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&gpio, GPIO_OUTPUT_HIGH);
    if (ret < 0)
    {
        return ret;
    }

    wake_bridge();

    ret = pinctrl_apply_state(uart_pinctrl, PINCTRL_STATE_DEFAULT);
    if (ret < 0)
    {
        return ret;
    }

    ret = uart_init();
    if (ret < 0)
    {
        return ret;
    }

    ret = wake_stack();
    if (ret < 0)
    {
        return ret;
    }

    k_msleep(15);

    ret = auto_address();
    if (ret < 0)
    {
        return ret;
    }

    return 0;
}

static void line_drive_high(void)
{
    gpio_pin_set_dt(&gpio, 1);
}

static void line_drive_low(void)
{
    gpio_pin_set_dt(&gpio, 0);
}

static void send_single_wake_ping(void)
{
    line_drive_high();
    k_usleep(BQ_PRE_PULSE_IDLE_US);

    line_drive_low();
    k_usleep(BQ_WAKE_PULSE_US);

    line_drive_high();
}

static void wake_bridge(void)
{
    send_single_wake_ping();
    k_usleep(BQ_WAKE_TO_WAKE_DELAY_US);

    send_single_wake_ping();
    k_usleep(BQ_WAKE_TO_WAKE_DELAY_US);
}

static int wake_stack(void)
{
    uint8_t data = BQ79600_CONTROL1_SEND_WAKE;

    return write_reg(SINGLE_WRITE, BRIDGE_ADDR, BQ79600_REG_CONTROL1, &data, 1U);
}

static int auto_address(void)
{
    uint8_t data;
    uint8_t rx_buf[ONE_BYTE_READ_FRAME_LEN];
    int ret;

    for (uint16_t reg = BQ79616_REG_OTP_ECC_DATAIN1; reg <= BQ79616_REG_OTP_ECC_DATAIN8; reg++)
    {
        data = OTP_DUMMY_VALUE;
        ret = write_reg(STACK_WRITE, 0U, reg, &data, 1U);
        if (ret < 0)
        {
            return ret;
        }

        k_msleep(1);
    }

    data = BQ79616_CONTROL1_AUTO_ADDR;
    ret = write_reg(BROADCAST_WRITE, 0U, BQ79616_REG_CONTROL1, &data, 1U);
    if (ret < 0)
    {
        return ret;
    }
    k_msleep(1);

    for (uint8_t addr = 0U; addr <= NUM_STACK_DEVICES; addr++)
    {
        data = addr;
        ret = write_reg(BROADCAST_WRITE, 0U, BQ79616_REG_DIR0_ADDR, &data, 1U);
        if (ret < 0)
        {
            return ret;
        }

        k_msleep(1);
    }

    data = BQ79616_COMM_CTRL_STACK;
    ret = write_reg(BROADCAST_WRITE, 0U, BQ79616_REG_COMM_CTRL, &data, 1U);
    if (ret < 0)
    {
        return ret;
    }
    k_msleep(1);

    data = BQ79616_COMM_CTRL_TOP_STACK;
    ret = write_reg(SINGLE_WRITE, NUM_STACK_DEVICES, BQ79616_REG_COMM_CTRL, &data, 1U);
    if (ret < 0)
    {
        return ret;
    }
    k_msleep(1);

    for (uint16_t reg = BQ79616_REG_OTP_ECC_DATAIN1;
         reg <= BQ79616_REG_OTP_ECC_DATAIN8;
         reg++)
    {
        ret = read_reg(STACK_READ, 0U, reg, rx_buf, sizeof(rx_buf), 1U);
        if (ret < 0)
        {
            return ret;
        }

        k_msleep(1);
    }

    return 0;
}