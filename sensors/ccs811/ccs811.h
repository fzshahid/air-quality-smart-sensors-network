#ifndef CCS811_H
#define CCS811_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>

struct ccs811_data
{
    const struct device *i2c_dev;
    uint8_t address;
};

int ccs811_init(struct ccs811_data *data, const struct device *i2c_dev, uint8_t address);
bool ccs811_data_ready(struct ccs811_data *data);
int ccs811_read(struct ccs811_data *data, uint16_t *eco2, uint16_t *tvoc);

#endif