#include "uart.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#define UART_NODE DT_NODELABEL(usart1)

#define BAUDRATE UINT32_C(1000000)
#define TX_TIMEOUT_US SYS_FOREVER_US
#define RX_TIMEOUT_US 500
#define TX_WAIT_MS 50
#define RX_WAIT_MS 100

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

static K_SEM_DEFINE(tx_done_sem, 0, 1);
static K_SEM_DEFINE(rx_done_sem, 0, 1);

static volatile size_t rx_count = 0;
static volatile size_t expected_rx_len_current = 0;
static volatile bool rx_enabled = false;

static void uart_flush_rx(void);
static void uart_callback(const struct device *dev, struct uart_event *evt, void *user_data);
static void uart_reset_state(uint8_t *rx_buf, size_t rx_len);

static void uart_flush_rx(void)
{
    unsigned char discarded_byte;

    while (uart_poll_in(uart_dev, &discarded_byte) == 0)
    {
        /* Discard stale bytes */
    }
}

static void uart_reset_state(uint8_t *rx_buf, size_t rx_len)
{
    rx_enabled = false;
    rx_count = 0;
    expected_rx_len_current = 0;

    if ((rx_buf != NULL) && (rx_len > 0U))
    {
        memset(rx_buf, 0, rx_len);
    }

    while (k_sem_take(&tx_done_sem, K_NO_WAIT) == 0)
    {
        /* Drain semaphore tokens */
    }

    while (k_sem_take(&rx_done_sem, K_NO_WAIT) == 0)
    {
        /* Drain semaphore tokens */
    }
}

int uart_init(void)
{
    if (!device_is_ready(uart_dev))
    {
        return -ENODEV;
    }

    struct uart_config config = {
        .baudrate = BAUDRATE,
        .parity = UART_CFG_PARITY_NONE,
        .stop_bits = UART_CFG_STOP_BITS_1,
        .data_bits = UART_CFG_DATA_BITS_8,
        .flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
    };

    int ret = uart_configure(uart_dev, &config);
    if (ret < 0)
    {
        return ret;
    }

    ret = uart_callback_set(uart_dev, uart_callback, NULL);
    if (ret < 0)
    {
        return ret;
    }

    return 0;
}

int uart_transaction(const uint8_t *tx_buf, size_t tx_len, uint8_t *rx_buf, size_t rx_len, size_t expected_rx_len)
{
    int ret;

    uart_reset_state(rx_buf, rx_len);
    uart_flush_rx();

    expected_rx_len_current = expected_rx_len;

    if (expected_rx_len > 0U)
    {
        ret = uart_rx_enable(uart_dev, rx_buf, rx_len, RX_TIMEOUT_US);
        if (ret < 0)
        {
            return ret;
        }

        rx_enabled = true;
    }

    ret = uart_tx(uart_dev, tx_buf, tx_len, TX_TIMEOUT_US);
    if (ret < 0)
    {
        if (rx_enabled)
        {
            (void)uart_rx_disable(uart_dev);
        }

        return ret;
    }

    if (k_sem_take(&tx_done_sem, K_MSEC(TX_WAIT_MS)) != 0)
    {
        if (rx_enabled)
        {
            (void)uart_rx_disable(uart_dev);
        }

        return -ETIMEDOUT;
    }

    if (expected_rx_len > 0U)
    {
        if (k_sem_take(&rx_done_sem, K_MSEC(RX_WAIT_MS)) != 0)
        {
            if (rx_enabled)
            {
                (void)uart_rx_disable(uart_dev);
            }

            return -ETIMEDOUT;
        }

        if (rx_enabled)
        {
            (void)uart_rx_disable(uart_dev);
        }

        if (rx_count < expected_rx_len)
        {
            return -EMSGSIZE;
        }
    }

    return 0;
}

static void uart_callback(const struct device *dev, struct uart_event *evt, void *user_data)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    switch (evt->type)
    {
    case UART_TX_DONE:
    case UART_TX_ABORTED:
        k_sem_give(&tx_done_sem);
        break;

    case UART_RX_RDY:
        size_t end = evt->data.rx.offset + evt->data.rx.len;

        if (end > rx_count)
        {
            rx_count = end;
        }

        if (rx_count >= expected_rx_len_current)
        {
            k_sem_give(&rx_done_sem);
        }
        break;

    case UART_RX_DISABLED:
    case UART_RX_STOPPED:
        rx_enabled = false;
        break;

    default:
        break;
    }
}