#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <errno.h>

#define WAKE_PORT_NODE DT_NODELABEL(gpiob)
#define WAKE_PIN 6

#define BQ_PRE_PULSE_IDLE_MS 1
#define BQ_WAKE_PULSE_MS 2.75
#define BQ_WAKE_TO_WAKE_DELAY_MS 3.50
#define BQ_SHUTDOWN_PULSE_MS 8
#define BQ_POST_SHUTDOWN_DELAY_MS 20
#define BQ_LED_ON_HOLD_MS 3000
#define BQ_LED_OFF_HOLD_MS 3000

static const struct device *gpiob_device = DEVICE_DT_GET(WAKE_PORT_NODE);

static int bq_init_wake_pin(void)
{
    int ret;

    if (!device_is_ready(gpiob_device))
    {
        printk("GPIOB device not ready\n");
        return -ENODEV;
    }

    ret = gpio_pin_configure(gpiob_device, WAKE_PIN, GPIO_OUTPUT_HIGH);
    if (ret < 0)
    {
        printk("Failed to configure PB6: %d\n", ret);
        return ret;
    }

    return 0;
}

static void bq_line_idle_high(void)
{
    gpio_pin_set(gpiob_device, WAKE_PIN, 1);
}

static void bq_line_drive_low(void)
{
    gpio_pin_set(gpiob_device, WAKE_PIN, 0);
}

static void bq_send_single_wake_ping(void)
{
    bq_line_idle_high();
    k_msleep(BQ_PRE_PULSE_IDLE_MS);

    bq_line_drive_low();
    k_msleep(BQ_WAKE_PULSE_MS);
    bq_line_idle_high();
}

static void bq_send_wake_sequence(void)
{
    printk("WAKE ping 1\n");
    bq_send_single_wake_ping();

    k_msleep(BQ_WAKE_TO_WAKE_DELAY_MS);

    printk("WAKE ping 2\n");
    bq_send_single_wake_ping();

    k_msleep(BQ_WAKE_TO_WAKE_DELAY_MS);
}

static void bq_send_shutdown_ping(void)
{
    printk("SHUTDOWN ping\n");

    bq_line_idle_high();
    k_msleep(BQ_PRE_PULSE_IDLE_MS);

    bq_line_drive_low();
    k_msleep(BQ_SHUTDOWN_PULSE_MS);
    bq_line_idle_high();

    k_msleep(BQ_POST_SHUTDOWN_DELAY_MS);
}

int main(void)
{
    int ret = bq_init_wake_pin();
    if (ret < 0)
    {
        printk("Init failed: %d\n", ret);
        return 0;
    }

    printk("BQ79600 wake and shutdown loop starting...\n");

    while (1)
    {
        printk("\n--- Waking device ---\n");
        bq_send_wake_sequence();
        printk("Wake sequence done, LEDs on\n");

        k_msleep(BQ_LED_ON_HOLD_MS);

        printk("\n--- Sending shutdown ---\n");
        bq_send_shutdown_ping();
        printk("Shutdown ping done, LEDs off\n");

        k_msleep(BQ_LED_OFF_HOLD_MS);
    }

    return 0;
}