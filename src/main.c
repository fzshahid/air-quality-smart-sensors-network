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
#define DATA_SENDING_INTERVAL 1000
#define TEXTBUFFER_SIZE 30
#define BUTTON0_NODE DT_NODELABEL(button0) // DT_N_S_buttons_S_button_0
#define BUTTON1_NODE DT_NODELABEL(button1) // DT_N_S_buttons_S_button_0

static const struct gpio_dt_spec button0_spec = GPIO_DT_SPEC_GET(BUTTON0_NODE, gpios);
static const struct gpio_dt_spec button1_spec = GPIO_DT_SPEC_GET(BUTTON1_NODE, gpios);
static struct gpio_callback button_cb;

static struct k_timer send_timer;
static volatile bool function_running = false;

int16_t get_serial(int16_t error, int16_t serial_0, int16_t serial_1, int16_t serial_2);
void read_scd41();
static void coap_send_data_request(struct k_work *work);
static void coap_send_data_response_cb(void *p_context, otMessage *p_message,
                                       const otMessageInfo *p_message_info, otError result);

struct sensor_value temperature, humidity, co2, eco2, tvoc;
// struct ccs_data ;
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

        // uint16_t co2;
        // int32_t temperature;
        // int32_t humidity;

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

void read_ccs811(struct ccs811_data ccs811)
{
        // uint16_t eco2, tvoc;
        if (ccs811_data_ready(&ccs811))
        {
                if (ccs811_read(&ccs811, &eco2, &tvoc) == 0)
                {
                        printk("Data read from CCS811\n");
                        // printk("CCS811: \n");
                        // printk("eCO2: %d ppm, TVOC: %d ppb\n\n", eco2, tvoc);
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

void read_sps30(int16_t ret, struct sps30_measurement m)
{
        sensirion_sleep_usec(SPS30_MEASUREMENT_DURATION_USEC);
        ret = sps30_read_measurement(&m);
        if (ret < 0)
        {
                printk("error reading measurement\n");
        }
        else
        {
                printk("SPS30:\n"
                       "%0.2f pm1.0\n"
                       "%0.2f pm2.5\n"
                       "%0.2f pm4.0\n"
                       "%0.2f pm10.0\n"
                       "%0.2f nc0.5\n"
                       "%0.2f nc1.0\n"
                       "%0.2f nc2.5\n"
                       "%0.2f nc4.5\n"
                       "%0.2f nc10.0\n"
                       "%0.2f typical particle size\n\n",
                       m.mc_1p0, m.mc_2p5, m.mc_4p0, m.mc_10p0, m.nc_0p5, m.nc_1p0,
                       m.nc_2p5, m.nc_4p0, m.nc_10p0, m.typical_particle_size);
        }
}

static void coap_send_data_request(struct k_work *work)
{
        char sensors_data[200];
        otError error = OT_ERROR_NONE;
        otMessage *myMessage;
        otMessageInfo myMessageInfo;
        otInstance *myInstance = openthread_get_default_instance();
        const otMeshLocalPrefix *ml_prefix = otThreadGetMeshLocalPrefix(myInstance);
        uint8_t serverInterfaceID[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
        const char *serverIpAddr = "fd00:0:fb01:1:c9bd:dc9d:23e:82c5";
        const char *myTemperatureJson = "{\"temperature\": 23.32}";

        printk("Sending payload!\n");

        do
        {
                read_scd41();
                read_ccs811(ccs811);
                // double temp_cel = temperature / 1000.0;
                // double hum = humidity / 1000.0;
                // printk("CO2: %u ppm\n", sensor_value_to_double(&co2));
                // printk("Temperature: %.3f *C\n", sensor_value_to_double(&temperature) / 1000.0);
                // printk("Humidity: %.3f %%\n", sensor_value_to_double(&humidity) / 1000.0);
                printk("tvoc: %d %%\n", sensor_value_to_double(&tvoc));
                // k_sleep(K_SECONDS(5));
                // if (!success)
                // {
                //         continue;
                // }
                // read_ccs811(ccs811);
                // read_sps30(ret, m);

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
                error = otCoapMessageAppendContentFormatOption(myMessage,
                                                               OT_COAP_OPTION_CONTENT_FORMAT_JSON);
                if (error != OT_ERROR_NONE)
                {
                        break;
                }
                error = otCoapMessageSetPayloadMarker(myMessage);
                // , \"eCO2\": %.2f, \"tvoc\": %.2f
                snprintf(sensors_data, sizeof(sensors_data),
                         "{\"CO2\": %.2f ,\"Humidity\": %.2f, \"Temperature\": %.2f, \"eCO2\": %.2f}",
                         sensor_value_to_double(&co2),
                         sensor_value_to_double(&humidity) / 1000.0,
                         sensor_value_to_double(&temperature) / 1000.0,
                         sensor_value_to_double(&eco2));

                sensors_data[149] = '\0';

                if (error != OT_ERROR_NONE)
                {
                        break;
                }

                printk("%s\n", sensors_data);

                // char temperature_string[100] = {'h', 'e'};
                // const char *payload = "hello world";
                // snprintf(temperature_string, sizeof(temperature_string), "{\"temperature\": %.2f}", sensor_value_to_double(&temp));

                error = otMessageAppend(myMessage, sensors_data,
                                        strlen(sensors_data));
                if (error != OT_ERROR_NONE)
                {
                        break;
                }
                memset(&myMessageInfo, 0, sizeof(myMessageInfo));
                memset(sensors_data, 0, sizeof(sensors_data));
                myMessageInfo.mPeerPort = OT_DEFAULT_COAP_PORT;

                error = otIp6AddressFromString(serverIpAddr, &myMessageInfo.mPeerAddr);
                if (error != OT_ERROR_NONE)
                {
                        break;
                }

                error = otCoapSendRequest(myInstance, myMessage, &myMessageInfo,
                                          coap_send_data_response_cb, NULL);

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
}

K_WORK_DEFINE(sensor_work, coap_send_data_request);

static void coap_send_data_response_cb(void *p_context, otMessage *p_message,
                                       const otMessageInfo *p_message_info, otError result)
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
                printk("Submitting worker.");
                k_work_submit(&sensor_work);
        }
}

int main(void)
{
        // --------------------------Initializing SCD41--------------------------
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
                printk("Error executing scd4x_start_periodic_measurement(): %i\n",
                       error);
        }

        // // --------------------------Initializing ccs811--------------------------

        printk("Initializing ccs811\n");

        static const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
        // struct ccs811_data ccs811;
        // struct sps30_measurement m;
        // int16_t ret;

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

        // // --------------------------Initializing sps30--------------------------

        // printk("Initializing sps30\n");

        // uint8_t fw_major;
        // uint8_t fw_minor;
        // ret = sps30_read_firmware_version(&fw_major, &fw_minor);
        // if (ret)
        // {
        //         printk("error reading firmware version\n");
        // }
        // else
        // {
        //         printk("FW: %u.%u\n", fw_major, fw_minor);
        // }

        // char serial_number[SPS30_MAX_SERIAL_LEN];
        // ret = sps30_get_serial(serial_number);
        // if (ret)
        // {
        //         printk("error reading serial number\n");
        // }
        // else
        // {
        //         printk("Serial Number: %s\n\n", serial_number);
        // }

        // ret = sps30_start_measurement();
        // if (ret < 0)
        //         printk("error starting measurement\n");

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
