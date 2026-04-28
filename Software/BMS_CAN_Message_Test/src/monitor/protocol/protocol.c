#include "protocol.h"

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#define RX_THREAD_STACK_SIZE 512
#define RX_THREAD_PRIORITY 2
#define STATE_POLL_THREAD_STACK_SIZE 512
#define STATE_POLL_THREAD_PRIORITY 2
#define PROTECT_MSG_ID 0x010
#define PROTECT_BYTES 0x1
#define POWER_LIM_MSG_ID 0x250
#define POWER_LIM_BYTES 0x9
#define SEQUENCE_MSG_ID 0x251
#define SEQUENCE_BYTES 0x1
#define MONITOR_MSG_ID 0x252
#define MONITOR_BYTES 20
#define SLEEP_TIME K_MSEC(250)

K_THREAD_STACK_DEFINE(rx_thread_stack, RX_THREAD_STACK_SIZE);

static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

/*
CAN_MSGQ_DEFINE(protect_msgq, 2);
CAN_MSGQ_DEFINE(power_lim_msgq, 2);
CAN_MSGQ_DEFINE(sequence_msgq, 2);
 */
CAN_MSGQ_DEFINE(monitor_msgq, 2);


void tx_irq_callback(const struct device *dev, int error, void *arg)
{
    char *sender = (char *)arg;

    ARG_UNUSED(dev);

    if (error != 0) {
        printf("Callback! error-code: %d\nSender: %s\n",
               error, sender);
    }
}

char *state_to_str(enum can_state state)
{
    switch (state) {
        case CAN_STATE_ERROR_ACTIVE:
            return "error-active";
        case CAN_STATE_ERROR_WARNING:
            return "error-warning";
        case CAN_STATE_ERROR_PASSIVE:
            return "error-passive";
        case CAN_STATE_BUS_OFF:
            return "bus-off";
        case CAN_STATE_STOPPED:
            return "stopped";
        default:
            return "unknown";
    }
}

void poll_state_thread(void *unused1, void *unused2, void *unused3)
{
    struct can_bus_err_cnt err_cnt = {0, 0};
    struct can_bus_err_cnt err_cnt_prev = {0, 0};
    enum can_state state_prev = CAN_STATE_ERROR_ACTIVE;
    enum can_state state;
    int err;

    while (1) {
        err = can_get_state(can_dev, &state, &err_cnt);
        if (err != 0) {
            printf("Failed to get CAN controller state: %d", err);
            k_sleep(K_MSEC(100));
            continue;
        }

        if (err_cnt.tx_err_cnt != err_cnt_prev.tx_err_cnt ||
            err_cnt.rx_err_cnt != err_cnt_prev.rx_err_cnt ||
            state_prev != state) {

            err_cnt_prev.tx_err_cnt = err_cnt.tx_err_cnt;
        err_cnt_prev.rx_err_cnt = err_cnt.rx_err_cnt;
        state_prev = state;
        printf("state: %s\n"
        "rx error count: %d\n"
        "tx error count: %d\n",
        state_to_str(state),
               err_cnt.rx_err_cnt, err_cnt.tx_err_cnt);
            } else {
                k_sleep(K_MSEC(100));
            }
    }
}

void state_change_work_handler(struct k_work *work)
{
    printf("State Change ISR\nstate: %s\n"
    "rx error count: %d\n"
    "tx error count: %d\n",
    state_to_str(current_state),
           current_err_cnt.rx_err_cnt, current_err_cnt.tx_err_cnt);
}

void state_change_callback(const struct device *dev, enum can_state state,
                           struct can_bus_err_cnt err_cnt, void *user_data)
{
    struct k_work *work = (struct k_work *)user_data;

    ARG_UNUSED(dev);

    current_state = state;
    current_err_cnt = err_cnt;
    k_work_submit(work);
}
