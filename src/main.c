#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include "../sensors/ccs811/ccs811.h"
#include "../sensors/sps30/sps30.h"

#define I2C_DEV DT_LABEL(DT_ALIAS(i2c0))
#define I2C_NODE DT_NODELABEL(i2c0)
#define I2C_DEV_LABEL DT_LABEL(DT_NODELABEL(i2c0))
#define CCS811_I2C_ADDRESS 0x5A

int main(void)
{
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

        while (1)
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
                k_sleep(K_SECONDS(10));
        }
        return 0;
}