/*
 * Test Code for the Battery Management System node on the CAN Bus
 * by Jose Ramirez from the BMS Capstone Team
 * Sponser: Viking Motorsports
 *
 * Code is Open Source licensed under GNU GENERAL PUBLIC LICENSE v2
 *
 */

#include <stdint.h>
#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/byteorder.h>

#define RX_THREAD_STACK_SIZE 512
#define RX_THREAD_PRIORITY 2
#define STATE_POLL_THREAD_STACK_SIZE 512
#define STATE_POLL_THREAD_PRIORITY 2
#define PROTECT_MSG_ID 0x010
#define PROTECT_BYTES 0x1
#define POWER_LIM_MSG_ID 0x250
#define POWER_LIM_BYTES 0x
#define SEQUENCE_MSG_ID 0x251
#define SEQUENCE_BYTES 1
#define MONITOR_MSG_ID 0x252
#define MONITOR_BYTES 20
#define SLEEP_TIME K_MSEC(250)

#ifdef RECIEVER
K_THREAD_STACK_DEFINE(rx_thread_stack, RX_THREAD_STACK_SIZE);
#endif

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

struct k_thread rx_thread_data;
struct k_thread poll_state_thread_data;
struct k_work_poll change_led_work;
struct k_work state_change_work;
enum can_state current_state;
struct can_bus_err_cnt current_err_cnt;

#ifdef RECIEVER
/*
CAN_MSGQ_DEFINE(protect_msgq, 2);
CAN_MSGQ_DEFINE(power_lim_msgq, 2);
CAN_MSGQ_DEFINE(sequence_msgq, 2);
*/
CAN_MSGQ_DEFINE(monitor_msgq, 2);
#endif

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

int main(void)
{
	uint16_t k_tid_t rx_tid, get_state_tid;
	uint8_t monitor_SOH, monitor_SOC;
	int ret;

	if (!device_is_ready(can_dev)) {
		printf("CAN: Device %s not ready.\n", can_dev->name);
		return 0;
	}

	ret = can_set_mode(can_dev, CAN_MODE_FD);
	if (ret != 0) {
		printf("Error setting CAN mode [%d]", ret);
		return 0;
	}

	ret = can_start(can_dev);
	if (ret != 0) {
		printf("Error starting CAN controller [%d]", ret);
		return 0;
	}

	rx_tid = k_thread_create(&rx_thread_data, rx_thread_stack,
				 K_THREAD_STACK_SIZEOF(rx_thread_stack),
				 rx_thread, NULL, NULL, NULL,
				 RX_THREAD_PRIORITY, 0, K_NO_WAIT);
	if (!rx_tid) {
		printf("ERROR spawning rx thread\n");
	}

	can_set_state_change_callback(can_dev, state_change_callback, &state_change_work);

	printf("Finished init.\n");

	while (1) {
		// Data structure for CAN messages
		/*
		struct can_frame bms_protect_frame = {
			.flags = CAN_FRAME_FDF,
			.id = PROTECT_MSG_ID,
			.dlc = PROTECT_BYTES
		};
		struct can_frame bms_power_lim_frame = {
			.flags = CAN_FRAME_FDF,
			.id = POWER_LIM_MSG_ID,
			.dlc = POWER_LIM_BYTES
		};
		struct can_frame bms_sequence_frame = {
			.flags = CAN_FRAME_FDF,
			.id = SEQUENCE_MSG_ID,
			.dlc = SEQUENCE_BYTES
		};
		*/
		struct can_frame bms_monitor_frame = {
			.flags = CAN_FRAME_FDF,
			.id = MONITOR_MSG_ID,
			.dlc = MONITOR_BYTES
		};

		gpio_pin_set(led.port, led.pin, 0); // CAN Transmittion Begins
		/*
		// Montioring Message
		UNALIGNED_PUT(sys_cpu_to_be16(monitor_value),
					  (uint16_t *)&monitor_frame.data[0]);

		// This sending call is blocking until the message is sent.
		can_send(can_dev, &monitor_frame, K_MSEC(100), NULL, NULL);
		k_sleep(SLEEP_TIME);
		*/
		// Montioring Message
		UNALIGNED_PUT(sys_cpu_to_be16(monitor_value),
			      (uint16_t *)&monitor_frame.data);

		// This sending call is blocking until the message is sent.
		can_send(can_dev, &monitor_frame, K_MSEC(100), NULL, NULL);
		gpio_pin_set(led.port, led.pin, 1); // CAN Transmittion Finished
		k_sleep(SLEEP_TIME);
	}
}
