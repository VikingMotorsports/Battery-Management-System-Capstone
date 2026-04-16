/*
 * Test Code for the Battery Management System node on the CAN Bus
 * by Jose Ramirez from the BMS Capstone Team
 * Sponser: Viking Motorsports
 *
 * Code is Open Source licensed under GNU GENERAL PUBLIC LICENSE v2
 *
 */

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
#define PROTECT_BYTES 1
#define POWER_LIM_MSG_ID 0x250
#define POWER_LIM_BYTES 12
#define SEQUENCE_MSG_ID 0x251
#define SEQUENCE_BYTES 1
#define MONITOR_MSG_ID 0x252
#define MONITOR_BYTES 20
#define SLEEP_TIME K_MSEC(250)

K_THREAD_STACK_DEFINE(rx_thread_stack, RX_THREAD_STACK_SIZE);

const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

struct k_thread rx_thread_data;
struct k_thread poll_state_thread_data;
struct k_work_poll change_led_work;
struct k_work state_change_work;
enum can_state current_state;
struct can_bus_err_cnt current_err_cnt;

CAN_MSGQ_DEFINE(protect_msgq, 2);
CAN_MSGQ_DEFINE(power_lim_msgq, 2);
CAN_MSGQ_DEFINE(sequence_msgq, 2);
CAN_MSGQ_DEFINE(monitor_msgq, 2);

// Data structure for CAN messages
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
struct can_frame bms_monitor_frame = {
	.flags = CAN_FRAME_FDF,
	.id = MONITOR_MSG_ID,
	.dlc = MONITOR_BYTES
};

void tx_irq_callback(const struct device *dev, int error, void *arg)
{
	char *sender = (char *)arg;

	ARG_UNUSED(dev);

	if (error != 0) {
		printf("Callback! error-code: %d\nSender: %s\n",
		       error, sender);
	}
}
/*
void rx_thread(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);
	const struct can_filter p_filter = {
		.flags = CAN_FRAME_FDF,
		.id = MONITOR_MSG_ID
	};
	const struct can_filter pl_filter = {
		.flags = CAN_FRAME_FDF,
		.id = POWER_LIM_MSG_ID
	};
	const struct can_filter s_filter = {
		.flags = CAN_FRAME_FDF,
		.id = MONITOR_MSG_ID
	};
	const struct can_filter m_filter = {
		.flags = CAN_FRAME_FDF,
		.id = MONITOR_MSG_ID
	};
	struct can_frame frame;
	int filter_id [4];
	uint16_t fault_state;

	filter_id[0] = can_add_rx_filter_msgq(can_dev, &protect_msgq, &p_filter);
	printf("Protection id: %d\n", filter_id[0]);
	filter_id[1] = can_add_rx_filter_msgq(can_dev, &power_lim_msgq, &pl_filter);
	printf("Power Lim id: %d\n", filter_id[1]);
	filter_id[2] = can_add_rx_filter_msgq(can_dev, &sequence_msgq, &s_filter);
	printf("Sequence id: %d\n", filter_id[2]);
	filter_id[3] = can_add_rx_filter_msgq(can_dev, &monitor_msgq, &m_filter);
	printf("Monitor id: %d\n", filter_id[3]);

	while (1) {
		// Protect Frame
		k_msgq_get(&protect_msgq, &frame, K_FOREVER);

		if (IS_ENABLED(CONFIG_CAN_ACCEPT_RTR) && (frame.flags & CAN_FRAME_RTR) != 0U) {
			continue;
		}

		if (frame.dlc != 1U) {
			printf("Wrong data length: %u\n", frame.dlc);
			continue;
		}
		\
		fault_state = sys_be16_to_cpu(UNALIGNED_GET((uint16_t *)&frame.data)) >> 6;
		printf("Fault State received: %u",
			   fault_state);

		if (fault_state == 2U)
			printf("%X",sys_be16_to_cpu(UNALIGNED_GET((uint16_t *)&frame.data)) & 0x3F);
		else
			printf("\n");

		// Power Limits Frame
		k_msgq_get(&power_lim_msgq, &frame, K_FOREVER);

		if (IS_ENABLED(CONFIG_CAN_ACCEPT_RTR) && (frame.flags & CAN_FRAME_RTR) != 0U) {
			continue;
		}

		if (frame.dlc != 12U) {
			printf("Wrong data length: %u\n", frame.dlc);
			continue;
		}

		printf("Counter received: %u\n",
			   sys_be16_to_cpu(UNALIGNED_GET((uint16_t *)&frame.data_32)));

		// Sequencing Frame
		k_msgq_get(&sequence_msgq, &frame, K_FOREVER);

		if (IS_ENABLED(CONFIG_CAN_ACCEPT_RTR) && (frame.flags & CAN_FRAME_RTR) != 0U) {
			continue;
		}

		if (frame.dlc != 1U) {
			printf("Wrong data length: %u\n", frame.dlc);
			continue;
		}

		printf("Counter received: %X\n",
			   sys_be16_to_cpu(UNALIGNED_GET((uint16_t *)&frame.data)));

		// Montiorinf Frame
		k_msgq_get(&monitor_msgq, &frame, K_FOREVER);

		if (IS_ENABLED(CONFIG_CAN_ACCEPT_RTR) && (frame.flags & CAN_FRAME_RTR) != 0U) {
			continue;
		}

		if (frame.dlc != 20U) {
			printf("Wrong data length: %u\n", frame.dlc);
			continue;
		}

		printf("Counter received: %X\n",
		       sys_be16_to_cpu(UNALIGNED_GET((uint16_t *)&frame.data)));
	}
}
*/
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
#ifdef CONFIG_LOOPBACK_MODE
	ret = can_set_mode(can_dev, CAN_MODE_LOOPBACK);
	if (ret != 0) {
		printf("Error setting CAN mode [%d]", ret);
		return 0;
	}
#endif
	ret = can_start(can_dev);
	if (ret != 0) {
		printf("Error starting CAN controller [%d]", ret);
		return 0;
	}

	if (led.port != NULL) {
		if (!gpio_is_ready_dt(&led)) {
			printf("LED: Device %s not ready.\n",
			       led.port->name);
			return 0;
		}
		ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_HIGH);
		if (ret < 0) {
			printf("Error setting LED pin to output mode [%d]",
			       ret);
			led.port = NULL;
		}
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
		gpio_pin_set(led.port, led.pin, 0); // CAN Transmittion Begins

		// Montioring Message
		UNALIGNED_PUT(sys_cpu_to_be16(monitor_value),
					  (uint16_t *)&monitor_frame.data[0]);

		/* This sending call is blocking until the message is sent. */
		can_send(can_dev, &monitor_frame, K_MSEC(100), NULL, NULL);
		k_sleep(SLEEP_TIME);

		// Montioring Message
		UNALIGNED_PUT(sys_cpu_to_be16(monitor_value),
			      (uint16_t *)&monitor_frame.data[0]);

		/* This sending call is blocking until the message is sent. */
		can_send(can_dev, &monitor_frame, K_MSEC(100), NULL, NULL);
		gpio_pin_set(led.port, led.pin, 1); // CAN Transmittion Finished
		k_sleep(SLEEP_TIME);
	}
}
