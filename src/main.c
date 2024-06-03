#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include "ccs811.h"

// Using the DT_ALIAS macro to reference the I2C0 device
#define I2C_DEV DT_LABEL(DT_ALIAS(i2c0))

#define I2C_NODE DT_NODELABEL(i2c0) // DT_N_S_soc_S_i2c_40003000

#define I2C_DEV_LABEL DT_LABEL(DT_NODELABEL(i2c0))

#define CCS811_I2C_ADDRESS 0x5A

int main(void)
{
        static const struct device *i2c_dev = DEVICE_DT_GET(I2C_NODE);

        if (i2c_dev == NULL)
        {
                printk("Failed to get I2C device\n");
                return -1;
        }

        printk("I2C device found\n");

        struct ccs811_data ccs811;

        if (ccs811_init(&ccs811, i2c_dev, CCS811_I2C_ADDRESS) != 0)
        {
                printk("Failed to initialize CCS811 sensor\n");
                return -1;
        }

        printk("CCS811 initialized\n");

        while (1)
        {
                uint16_t eco2, tvoc;

                if (ccs811_data_ready(&ccs811))
                {
                        if (ccs811_read(&ccs811, &eco2, &tvoc) == 0)
                        {
                                printk("eCO2: %d ppm, TVOC: %d ppb\n", eco2, tvoc);
                        }
                        else
                        {
                                printk("Failed to read CCS811 sensor data\n");
                        }
                }
                else
                {
                        printk("CCS811 data not ready :(\n");
                }

                k_sleep(K_SECONDS(1));
        }

        return 0;
}
