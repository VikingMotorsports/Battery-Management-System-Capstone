#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>


struct k_thread rx_thread_data;
struct k_thread poll_state_thread_data;
struct k_work_poll change_led_work;
struct k_work state_change_work;
enum can_state current_state;
struct can_bus_err_cnt current_err_cnt;

void tx_irq_callback(const struct device *dev, int error, void *arg);
char *state_to_str(enum can_state state);


#endif
