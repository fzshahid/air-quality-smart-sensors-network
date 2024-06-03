// src/ccs811.c

#include "ccs811.h"
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

// #define CCS811_STATUS_REGISTER 0x00
// #define CCS811_APP_START 0xF4
// #define CCS811_MEAS_MODE 0x01

// int ccs811_init(struct ccs811_data *data, const struct device *i2c_dev, uint8_t address)
// {
//     data->i2c_dev = i2c_dev;
//     data->address = address;

//     uint8_t status;

//     // Check sensor status
//     if (i2c_reg_read_byte(data->i2c_dev, data->address, CCS811_STATUS_REGISTER, &status) < 0)
//     {
//         printk("Failed to read status register\n");
//         return -EIO;
//     }
//     printk("Initial status: 0x%x\n", status);

//     // Ensure sensor is in boot mode
//     if ((status & 0x10) == 0)
//     {
//         printk("Sensor is not in boot mode\n");
//         return -EIO;
//     }

//     // Send application start command
//     uint8_t app_start_cmd = CCS811_APP_START;
//     if (i2c_write(data->i2c_dev, &app_start_cmd, 1, data->address) < 0)
//     {
//         printk("Failed to send application start command\n");
//         return -EIO;
//     }

//     k_sleep(K_MSEC(100));

//     // Check status after app start
//     if (i2c_reg_read_byte(data->i2c_dev, data->address, CCS811_STATUS_REGISTER, &status) < 0)
//     {
//         printk("Failed to read status after app start\n");
//         return -EIO;
//     }
//     printk("Status after app start: 0x%x\n", status);

//     if ((status & 0x80) == 0)
//     {
//         printk("Sensor failed to start application mode\n");
//         return -EIO;
//     }

//     // Set measurement mode
//     uint8_t meas_mode[] = {CCS811_MEAS_MODE, 0x10}; // Drive Mode 1 (constant power mode, measurement every second)
//     if (i2c_write(data->i2c_dev, meas_mode, sizeof(meas_mode), data->address) < 0)
//     {
//         printk("Failed to set measurement mode\n");
//         return -EIO;
//     }

//     return 0;
// }

// bool ccs811_data_ready(struct ccs811_data *data)
// {
//     uint8_t status;
//     if (i2c_reg_read_byte(data->i2c_dev, data->address, CCS811_STATUS_REGISTER, &status) < 0)
//     {
//         printk("Failed to read status for data ready check\n");
//         return false;
//     }
//     printk("Status for data ready check: 0x%x\n", status);

//     return (status & 0x08);
// }

// int ccs811_read(struct ccs811_data *data, uint16_t *eco2, uint16_t *tvoc)
// {
//     uint8_t buffer[5];
//     if (i2c_burst_read(data->i2c_dev, data->address, 0x02, buffer, 5) < 0)
//     {
//         printk("Failed to read sensor data\n");
//         return -EIO;
//     }

//     *eco2 = ((uint16_t)buffer[0] << 8) | buffer[1];
//     *tvoc = ((uint16_t)buffer[2] << 8) | buffer[3];

//     return 0;
// }

#define CCS811_STATUS_REGISTER 0x00
#define CCS811_APP_START 0xF4
#define CCS811_MEAS_MODE 0x01

int ccs811_init(struct ccs811_data *data, const struct device *i2c_dev, uint8_t address)
{
    data->i2c_dev = i2c_dev;
    data->address = address;

    uint8_t status;

    // Check sensor status
    if (i2c_reg_read_byte(data->i2c_dev, data->address, CCS811_STATUS_REGISTER, &status) < 0)
    {
        printk("Failed to read status register\n");
        return -EIO;
    }
    printk("Initial status: 0x%x\n", status);

    // Ensure sensor is in boot mode
    if ((status & 0x10) == 0)
    {
        printk("Sensor is not in boot mode\n");
        return -EIO;
    }

    // Send application start command
    uint8_t app_start_cmd = CCS811_APP_START;
    if (i2c_write(data->i2c_dev, &app_start_cmd, 1, data->address) < 0)
    {
        printk("Failed to send application start command\n");
        return -EIO;
    }

    k_sleep(K_MSEC(100));

    // Check status after app start
    if (i2c_reg_read_byte(data->i2c_dev, data->address, CCS811_STATUS_REGISTER, &status) < 0)
    {
        printk("Failed to read status after app start\n");
        return -EIO;
    }
    printk("Status after app start: 0x%x\n", status);

    if ((status & 0x80) == 0)
    {
        printk("Sensor failed to start application mode\n");
        return -EIO;
    }

    // Set measurement mode
    uint8_t meas_mode[] = {CCS811_MEAS_MODE, 0x10}; // Drive Mode 1 (constant power mode, measurement every second)
    if (i2c_write(data->i2c_dev, meas_mode, sizeof(meas_mode), data->address) < 0)
    {
        printk("Failed to set measurement mode\n");
        return -EIO;
    }

    return 0;
}

bool ccs811_data_ready(struct ccs811_data *data)
{
    uint8_t status;
    if (i2c_reg_read_byte(data->i2c_dev, data->address, CCS811_STATUS_REGISTER, &status) < 0)
    {
        printk("Failed to read status for data ready check\n");
        return false;
    }
    // printk("Status for data ready check: 0x%x\n", status);

    return (status & 0x08);
}

int ccs811_read(struct ccs811_data *data, uint16_t *eco2, uint16_t *tvoc)
{
    uint8_t buffer[5];
    if (i2c_burst_read(data->i2c_dev, data->address, 0x02, buffer, 5) < 0)
    {
        printk("Failed to read sensor data\n");
        return -EIO;
    }

    *eco2 = ((uint16_t)buffer[0] << 8) | buffer[1];
    *tvoc = ((uint16_t)buffer[2] << 8) | buffer[3];

    return 0;
}
