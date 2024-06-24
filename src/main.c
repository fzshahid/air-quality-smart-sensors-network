#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include "ccs811.h"
#include "sps30.h"

// #include "sps30.c"
// #include "hal.c"

// Using the DT_ALIAS macro to reference the I2C0 device
#define I2C_DEV DT_LABEL(DT_ALIAS(i2c0))

#define I2C_NODE DT_NODELABEL(i2c0) // DT_N_S_soc_S_i2c_40003000

#define I2C_DEV_LABEL DT_LABEL(DT_NODELABEL(i2c0))

#define CCS811_I2C_ADDRESS 0x5A

#include <stdio.h> // printk

/**
 * TO USE CONSOLE OUTPUT (printk) PLEASE ADAPT TO YOUR PLATFORM:
 * #define printk(...)
 */

int main(void)
{
        struct sps30_measurement m;
        int16_t ret;

        /* Initialize I2C bus */
        sensirion_i2c_init();

        /* Busy loop for initialization, because the main loop does not work without
         * a sensor.
         */
        while (sps30_probe() != 0)
        {
                printk("SPS sensor probing failed\n");
                sensirion_sleep_usec(1000000); /* wait 1s */
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
                printk("Serial Number: %s\n", serial_number);
        }

        ret = sps30_start_measurement();
        if (ret < 0)
                printk("error starting measurement\n");
        printk("measurements started\n");

        while (1)
        {
                sensirion_sleep_usec(SPS30_MEASUREMENT_DURATION_USEC); /* wait 1s */
                ret = sps30_read_measurement(&m);
                if (ret < 0)
                {
                        printk("error reading measurement\n");
                }
                else
                {
                        printk("measured values:\n"
                               "\t%0.2f pm1.0\n"
                               "\t%0.2f pm2.5\n"
                               "\t%0.2f pm4.0\n"
                               "\t%0.2f pm10.0\n"
                               "\t%0.2f nc0.5\n"
                               "\t%0.2f nc1.0\n"
                               "\t%0.2f nc2.5\n"
                               "\t%0.2f nc4.5\n"
                               "\t%0.2f nc10.0\n"
                               "\t%0.2f typical particle size\n\n",
                               m.mc_1p0, m.mc_2p5, m.mc_4p0, m.mc_10p0, m.nc_0p5, m.nc_1p0,
                               m.nc_2p5, m.nc_4p0, m.nc_10p0, m.typical_particle_size);
                }
        }

        return 0;
}