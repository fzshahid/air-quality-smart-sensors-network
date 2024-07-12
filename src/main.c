#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/openthread.h>
#include <zephyr/net/coap.h>
#include <openthread/thread.h>
#include <openthread/udp.h>
#include <openthread/coap.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include "../sensors/scd41/scd4x_i2c.h"
#include "../sensors/scd41/sensirion_common.h"
#include "../sensors/scd41/sensirion_i2c_hal.h"
#include "../sensors/ccs811/ccs811.h"
#include "../sensors/sps30/sps30.h"

#define SLEEP_TIME_MS 1000
#define DATA_SENDING_INTERVAL 10000
#define TEXTBUFFER_SIZE 30
#define BUTTON0_NODE DT_NODELABEL(button0)
#define BUTTON1_NODE DT_NODELABEL(button1)

static const struct gpio_dt_spec button0_spec = GPIO_DT_SPEC_GET(BUTTON0_NODE, gpios);
static const struct gpio_dt_spec button1_spec = GPIO_DT_SPEC_GET(BUTTON1_NODE, gpios);
static struct gpio_callback button_cb;

static struct k_timer send_timer;
static volatile bool function_running = false;

int16_t get_serial(int16_t error, int16_t serial_0, int16_t serial_1, int16_t serial_2);
void read_scd41();
void read_ccs811(struct ccs811_data *ccs811);
void read_sps30();

static void coap_send_data_request(struct k_work *work);
static void coap_send_data_response_cb(void *p_context, otMessage *p_message, const otMessageInfo *p_message_info, otError result);

struct sensor_value temperature, humidity, co2, eco2, tvoc;
struct ccs811_data ccs811;
struct sps30_measurement m;
int16_t ret;

#define I2C_DEV DT_LABEL(DT_ALIAS(i2c0))
#define I2C_NODE DT_NODELABEL(i2c0)
#define I2C_DEV_LABEL DT_LABEL(DT_NODELABEL(i2c0))
#define CCS811_I2C_ADDRESS 0x5A

int16_t get_serial(int16_t error, int16_t serial_0, int16_t serial_1, int16_t serial_2)
{
        error = scd4x_get_serial_number(&serial_0, &serial_1, &serial_2);
        return error;
}

void read_scd41()
{
        int16_t error;
        bool data_ready_flag = false;
        error = scd4x_get_data_ready_flag(&data_ready_flag);
        if (error)
        {
                printf("Error executing scd4x_get_data_ready_flag(): %i\n", error);
                return;
        }
        if (!data_ready_flag)
        {
                return;
        }

        error = scd4x_read_measurement(&co2, &temperature, &humidity);
        if (error)
        {
                printf("Error executing scd4x_read_measurement(): %i\n", error);
                return;
        }
        else if (sensor_value_to_double(&co2) == 0)
        {
                printf("Invalid sample detected, skipping.\n");
        }
}

void read_ccs811(struct ccs811_data *ccs811)
{
        if (ccs811_data_ready(ccs811))
        {
                if (ccs811_read(ccs811, &eco2, &tvoc) == 0)
                {
                        printk("Data read from CCS811\n");
                }
                else
                {
                        printk("Failed to read CCS811 sensor data\n");
                }
        }
        else
        {
                printk("CCS811 data not ready\n");
        }
}

void read_sps30()
{
        sensirion_sleep_usec(SPS30_MEASUREMENT_DURATION_USEC);
        ret = sps30_read_measurement(&m);
        if (ret < 0)
        {
                printk("Error reading measurement\n");
        }
        else
        {
                printk("SPS30:\n"
                       "PM1.0: %0.2f ug/m3\n"
                       "PM2.5: %0.2f ug/m3\n"
                       "PM4.0: %0.2f ug/m3\n"
                       "PM10: %0.2f ug/m3\n"
                       "NC0.5: %0.2f #/cm3\n"
                       "NC1.0: %0.2f #/cm3\n"
                       "NC2.5: %0.2f #/cm3\n"
                       "NC4.0: %0.2f #/cm3\n"
                       "NC10: %0.2f #/cm3\n"
                       "Typical Particle Size: %0.2f um\n\n",
                       m.mc_1p0, m.mc_2p5, m.mc_4p0, m.mc_10p0, m.nc_0p5, m.nc_1p0,
                       m.nc_2p5, m.nc_4p0, m.nc_10p0, m.typical_particle_size);
        }
}

static void coap_send_data_request(struct k_work *work)
{
        char sensors_data[160];
        otError error = OT_ERROR_NONE;
        otMessage *myMessage;
        otMessageInfo myMessageInfo;
        otInstance *myInstance = openthread_get_default_instance();
        const char *serverIpAddr = "fd00:0:fb01:1:c9bd:dc9d:23e:82c5";

        printk("Sending payload!\n");

        do
        {
                read_scd41();
                read_ccs811(&ccs811);
                read_sps30();

                myMessage = otCoapNewMessage(myInstance, NULL);
                if (myMessage == NULL)
                {
                        printk("Failed to allocate message for CoAP Request\n");
                        return;
                }
                otCoapMessageInit(myMessage, OT_COAP_TYPE_CONFIRMABLE, OT_COAP_CODE_PUT);
                error = otCoapMessageAppendUriPathOptions(myMessage, "storedata");
                if (error != OT_ERROR_NONE)
                {
                        break;
                }
                error = otCoapMessageAppendContentFormatOption(myMessage, OT_COAP_OPTION_CONTENT_FORMAT_JSON);
                if (error != OT_ERROR_NONE)
                {
                        break;
                }
                error = otCoapMessageSetPayloadMarker(myMessage);
                if (error != OT_ERROR_NONE)
                {
                        break;
                }
                printk("%.2f", sensor_value_to_double(&tvoc));
                const double calculated_humidity = sensor_value_to_double(&humidity) / 1000.0;
                const double calculated_temperature = sensor_value_to_double(&temperature) / 1000.0;
                snprintf(sensors_data, sizeof(sensors_data),
                         "{\"CO\":%.2f,\"Hm\":%.2f,\"Tp\":%.2f,\"eCO\":%.2f,\"1p0\":%.2f,\"2p5\":%.2f,\"4p0\":%.2f,\"10p0\":%.2f,\"ps\":%.2f}",
                         sensor_value_to_double(&co2),
                         calculated_humidity,
                         calculated_temperature,
                         sensor_value_to_double(&eco2),
                         m.mc_1p0,
                         m.mc_2p5, m.mc_4p0, m.mc_10p0, m.typical_particle_size);
                //  sensor_value_to_double(&tvoc),
                sensors_data[159] = '\0';

                error = otMessageAppend(myMessage, sensors_data, strlen(sensors_data));
                if (error != OT_ERROR_NONE)
                {
                        printk("Ops!");
                        break;
                }
                memset(&sensors_data, 0, sizeof(sensors_data));
                memset(&myMessageInfo, 0, sizeof(myMessageInfo));
                myMessageInfo.mPeerPort = OT_DEFAULT_COAP_PORT;

                error = otIp6AddressFromString(serverIpAddr, &myMessageInfo.mPeerAddr);
                if (error != OT_ERROR_NONE)
                {
                        break;
                }

                error = otCoapSendRequest(myInstance, myMessage, &myMessageInfo, coap_send_data_response_cb, NULL);

        } while (false);
        if (error != OT_ERROR_NONE)
        {
                printk("Failed to send CoAP Request: %d\n", error);
                otMessageFree(myMessage);
        }
        else
        {
                printk("CoAP data send.\n");
        }
        free(sensors_data);
}

K_WORK_DEFINE(sensor_work, coap_send_data_request);

static void coap_send_data_response_cb(void *p_context, otMessage *p_message, const otMessageInfo *p_message_info, otError result)
{
        if (result == OT_ERROR_NONE)
        {
                printk("Delivery confirmed.\n");
        }
        else
        {
                printk("Delivery not confirmed: %d\n", result);
        }
}

void coap_init()
{
        otInstance *p_instance = openthread_get_default_instance();
        otError error = otCoapStart(p_instance, OT_DEFAULT_COAP_PORT);
        if (error != OT_ERROR_NONE)
                printk("Failed to start Coap: %d\n", error);
        else
                printk("COAP init success!\n");
}

void button_pressed_cb(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
        if (pins & BIT(button0_spec.pin))
        {
                function_running = true;
                printk("Start sending data....\n");
                k_timer_start(&send_timer, K_NO_WAIT, K_MSEC(DATA_SENDING_INTERVAL));
        }
        else if (pins & BIT(button1_spec.pin))
        {
                function_running = false;
                printk("Stop sending data....\n");
                k_timer_stop(&send_timer);
        }
}

void send_timer_callback(struct k_timer *timer_id)
{
        if (function_running)
        {
                printk("Submitting worker.\n");
                k_work_submit(&sensor_work);
        }
}

int main(void)
{
        printk("Initializing SCD41\n");
        int16_t error = 0;
        sensirion_i2c_hal_init();
        scd4x_wake_up();
        scd4x_stop_periodic_measurement();
        scd4x_reinit();

        uint16_t serial_0 = 0;
        uint16_t serial_1 = 0;
        uint16_t serial_2 = 0;

        error = get_serial(error, serial_0, serial_1, serial_2);
        if (error)
        {
                printk("Error executing scd4x_get_serial_number(): %i\n", error);
        }
        else
        {
                printk("serial: 0x%04x%04x%04x\n", serial_0, serial_1, serial_2);
        }

        error = scd4x_start_periodic_measurement();
        if (error)
        {
                printk("Error executing scd4x_start_periodic_measurement(): %i\n", error);
        }

        printk("Initializing CCS811\n");

        static const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

        sensirion_i2c_init();

        if (i2c_dev == NULL)
        {
                printk("Failed to get I2C device\n");
                return -1;
        }
        printk("I2C device found\n");

        if (ccs811_init(&ccs811, i2c_dev, CCS811_I2C_ADDRESS) != 0)
        {
                printk("Failed to initialize CCS811 sensor\n");
        }
        printk("CCS811 initialized\n");

        while (sps30_probe() != 0)
        {
                printk("SPS30 sensor probing failed\n");
                k_sleep(K_SECONDS(1));
        }
        printk("SPS sensor probing successful\n");

        ret = sps30_start_measurement();
        if (ret < 0)
                printk("Error starting measurement\n");

        printk("Initializing COAP\n");
        coap_init();

        gpio_pin_configure_dt(&button0_spec, GPIO_INPUT);
        gpio_pin_interrupt_configure_dt(&button0_spec, GPIO_INT_EDGE_TO_ACTIVE);
        gpio_pin_configure_dt(&button1_spec, GPIO_INPUT);
        gpio_pin_interrupt_configure_dt(&button1_spec, GPIO_INT_EDGE_TO_ACTIVE);
        gpio_init_callback(&button_cb, button_pressed_cb, BIT(button0_spec.pin) | BIT(button1_spec.pin));
        gpio_add_callback(button0_spec.port, &button_cb);
        gpio_add_callback(button1_spec.port, &button_cb);

        printk("Setting timer.\n");
        k_timer_init(&send_timer, send_timer_callback, NULL);

        while (1)
        {
                k_msleep(SLEEP_TIME_MS);
        }
        return 0;
}
