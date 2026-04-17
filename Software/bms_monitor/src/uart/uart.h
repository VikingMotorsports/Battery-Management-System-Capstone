#ifndef UART_H
#define UART_H

#include <stddef.h>
#include <stdint.h>

int uart_init(void);
int uart_transaction(const uint8_t *tx_buf, size_t tx_len, uint8_t *rx_buf, size_t rx_len, size_t expected_rx_len);

#endif