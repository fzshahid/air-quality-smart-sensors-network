#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include "../sensors/ccs811/ccs811.h"
#include "../sensors/sps30/sps30.h"
#include "../sensors/scd41/scd4x_i2c.h"
#include "../sensors/scd41/sensirion_common.h"
#include "../sensors/scd41/sensirion_i2c_hal.h"

int16_t get_serial(int16_t error, int16_t serial_0, int16_t serial_1, int16_t serial_2);
bool read_scd41();

#define I2C_DEV DT_LABEL(DT_ALIAS(i2c0))
#define I2C_NODE DT_NODELABEL(i2c0)
#define I2C_DEV_LABEL DT_LABEL(DT_NODELABEL(i2c0))
#define CCS811_I2C_ADDRESS 0x5A

int16_t get_serial(int16_t error, int16_t serial_0, int16_t serial_1, int16_t serial_2)
{
        error = scd4x_get_serial_number(&serial_0, &serial_1, &serial_2);
        return error;
}

bool read_scd41()
{
        int16_t error;
        bool data_ready_flag = false;
        error = scd4x_get_data_ready_flag(&data_ready_flag);
        if (error)
        {
                printf("Error executing scd4x_get_data_ready_flag(): %i\n", error);
                return false;
        }
        if (!data_ready_flag)
        {
                return false;
        }

        uint16_t co2;
        int32_t temperature;
        int32_t humidity;
        error = scd4x_read_measurement(&co2, &temperature, &humidity);
        if (error)
        {
                printf("Error executing scd4x_read_measurement(): %i\n", error);
                return false;
        }
        else if (co2 == 0)
        {
                printf("Invalid sample detected, skipping.\n");
        }
        else
        {
                double temp_cel = temperature / 1000.0;
                double hum = humidity / 1000.0;
                printf("CO2: %u ppm\n", co2);
                printf("Temperature: %.3f *C\n", temp_cel);
                printf("Humidity: %.3f %%\n", hum);
        }
        return true;
}

void read_ccs811(struct ccs811_data ccs811)
{
        uint16_t eco2, tvoc;
        if (ccs811_data_ready(&ccs811))
        {
                if (ccs811_read(&ccs811, &eco2, &tvoc) == 0)
                {
                        printk("CCS811: \n");
                        printk("eCO2: %d ppm, TVOC: %d ppb\n\n", eco2, tvoc);
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

int main(void)
{
        // --------------------------Initializing SCD41--------------------------
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

        // --------------------------Initializing ccs811--------------------------

        static const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);
        struct ccs811_data ccs811;
        struct sps30_measurement m;
        int16_t ret;

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

        // --------------------------Initializing sps30--------------------------

        uint8_t fw_major;
        uint8_t fw_minor;
        ret = sps30_read_firmware_version(&fw_major, &fw_minor);
        if (ret)
        {
                printk("error reading firmware version\n");
        }
        else
        {
                printk("FW: %u.%u\n", fw_major, fw_minor);
        }

        char serial_number[SPS30_MAX_SERIAL_LEN];
        ret = sps30_get_serial(serial_number);
        if (ret)
        {
                printk("error reading serial number\n");
        }
        else
        {
                printk("Serial Number: %s\n\n", serial_number);
        }

        ret = sps30_start_measurement();
        if (ret < 0)
                printk("error starting measurement\n");

        printk("Attempting to read data from all three sensors.\n");

        while (1)
        {
                bool success = read_scd41();
                k_sleep(K_SECONDS(5));
                if (!success)
                {
                        continue;
                }
                read_ccs811(ccs811);
                read_sps30(ret, m);
                k_sleep(K_SECONDS(10));
        }
        return 0;
}
