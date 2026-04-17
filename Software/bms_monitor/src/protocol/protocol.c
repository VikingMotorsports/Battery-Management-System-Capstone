#include "protocol.h"
#include "uart.h"

#include <zephyr/sys/util.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>

#define SINGLE_READ_INIT 0x80U
#define SINGLE_WRITE_INIT 0x90U
#define STACK_READ_INIT 0xA0U
#define STACK_WRITE_INIT 0xB0U
#define BROADCAST_WRITE_INIT 0xD0U

#define MAX_DATA_BYTES 8U
#define MAX_FRAME_BYTES (MAX_DATA_BYTES + 6U)

static uint16_t crc16_ibm(const uint8_t *data, size_t len);
static size_t build_single_read_frame(uint8_t dev_addr, uint16_t reg_addr, uint8_t num_bytes, uint8_t *frame);
static size_t build_single_write_frame(uint8_t dev_addr, uint16_t reg_addr, const uint8_t *data, uint8_t num_bytes, uint8_t *frame);
static size_t build_stack_read_frame(uint16_t reg_addr, uint8_t num_bytes, uint8_t *frame);
static size_t build_stack_write_frame(uint16_t reg_addr, const uint8_t *data, uint8_t num_bytes, uint8_t *frame);
static size_t build_broadcast_write_frame(uint16_t reg_addr, const uint8_t *data, uint8_t num_bytes, uint8_t *frame);

int read_reg(read_mode_t mode, uint8_t dev_addr, uint16_t reg_addr, uint8_t *rx_buf, size_t rx_len, uint8_t num_bytes)
{
    uint8_t frame[MAX_FRAME_BYTES];
    size_t frame_len;
    size_t expected_rx_len;

    expected_rx_len = (size_t)(num_bytes + 6U);

    switch (mode)
    {
    case SINGLE_READ:
        frame_len = build_single_read_frame(dev_addr, reg_addr, num_bytes, frame);
        break;

    case STACK_READ:
        frame_len = build_stack_read_frame(reg_addr, num_bytes, frame);
        break;

    default:
        return -EINVAL;
    }

    return uart_transaction(frame, frame_len, rx_buf, rx_len, expected_rx_len);
}

int write_reg(write_mode_t mode, uint8_t dev_addr, uint16_t reg_addr, const uint8_t *data, uint8_t num_bytes)
{
    uint8_t frame[MAX_FRAME_BYTES];
    size_t frame_len;

    switch (mode)
    {
    case SINGLE_WRITE:
        frame_len = build_single_write_frame(dev_addr, reg_addr, data, num_bytes, frame);
        break;

    case STACK_WRITE:
        ARG_UNUSED(dev_addr);
        frame_len = build_stack_write_frame(reg_addr, data, num_bytes, frame);
        break;

    case BROADCAST_WRITE:
        ARG_UNUSED(dev_addr);
        frame_len = build_broadcast_write_frame(reg_addr, data, num_bytes, frame);
        break;

    default:
        return -EINVAL;
    }

    return uart_transaction(frame, frame_len, NULL, 0U, 0U);
}

static uint16_t crc16_ibm(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;

    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (int bit = 0; bit < 8; bit++)
        {
            if ((crc & 0x0001U) != 0U)
            {
                crc = (crc >> 1) ^ 0xA001U;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static size_t build_single_read_frame(uint8_t dev_addr, uint16_t reg_addr, uint8_t num_bytes, uint8_t *frame)
{
    uint16_t crc;

    frame[0] = SINGLE_READ_INIT;
    frame[1] = dev_addr;
    frame[2] = (uint8_t)(reg_addr >> 8);
    frame[3] = (uint8_t)(reg_addr & 0xFFU);
    frame[4] = (uint8_t)(num_bytes - 1U);

    crc = crc16_ibm(frame, 5U);
    frame[5] = (uint8_t)(crc & 0xFFU);
    frame[6] = (uint8_t)((crc >> 8) & 0xFFU);

    return 7U;
}

static size_t build_single_write_frame(uint8_t dev_addr, uint16_t reg_addr, const uint8_t *data, uint8_t num_bytes, uint8_t *frame)
{
    uint16_t crc;

    frame[0] = (uint8_t)(SINGLE_WRITE_INIT | ((num_bytes - 1U) & 0x07U));
    frame[1] = dev_addr;
    frame[2] = (uint8_t)(reg_addr >> 8);
    frame[3] = (uint8_t)(reg_addr & 0xFFU);

    for (uint8_t i = 0; i < num_bytes; i++)
    {
        frame[4U + i] = data[i];
    }

    crc = crc16_ibm(frame, 4U + num_bytes);
    frame[4U + num_bytes] = (uint8_t)(crc & 0xFFU);
    frame[5U + num_bytes] = (uint8_t)((crc >> 8) & 0xFFU);

    return (size_t)(6U + num_bytes);
}

static size_t build_stack_read_frame(uint16_t reg_addr, uint8_t num_bytes, uint8_t *frame)
{
    uint16_t crc;

    frame[0] = STACK_READ_INIT;
    frame[1] = (uint8_t)(reg_addr >> 8);
    frame[2] = (uint8_t)(reg_addr & 0xFFU);
    frame[3] = (uint8_t)(num_bytes - 1U);

    crc = crc16_ibm(frame, 4U);
    frame[4] = (uint8_t)(crc & 0xFFU);
    frame[5] = (uint8_t)((crc >> 8) & 0xFFU);

    return 6U;
}

static size_t build_stack_write_frame(uint16_t reg_addr, const uint8_t *data, uint8_t num_bytes, uint8_t *frame)
{
    uint16_t crc;

    frame[0] = (uint8_t)(STACK_WRITE_INIT | ((num_bytes - 1U) & 0x07U));
    frame[1] = (uint8_t)(reg_addr >> 8);
    frame[2] = (uint8_t)(reg_addr & 0xFFU);

    for (uint8_t i = 0; i < num_bytes; i++)
    {
        frame[3U + i] = data[i];
    }

    crc = crc16_ibm(frame, 3U + num_bytes);
    frame[3U + num_bytes] = (uint8_t)(crc & 0xFFU);
    frame[4U + num_bytes] = (uint8_t)((crc >> 8) & 0xFFU);

    return (size_t)(5U + num_bytes);
}

static size_t build_broadcast_write_frame(uint16_t reg_addr, const uint8_t *data, uint8_t num_bytes, uint8_t *frame)
{
    uint16_t crc;

    frame[0] = (uint8_t)(BROADCAST_WRITE_INIT | ((num_bytes - 1U) & 0x07U));
    frame[1] = (uint8_t)(reg_addr >> 8);
    frame[2] = (uint8_t)(reg_addr & 0xFFU);

    for (uint8_t i = 0; i < num_bytes; i++)
    {
        frame[3U + i] = data[i];
    }

    crc = crc16_ibm(frame, 3U + num_bytes);
    frame[3U + num_bytes] = (uint8_t)(crc & 0xFFU);
    frame[4U + num_bytes] = (uint8_t)((crc >> 8) & 0xFFU);

    return (size_t)(5U + num_bytes);
}