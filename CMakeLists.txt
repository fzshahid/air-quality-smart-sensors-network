cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(air-quality-smart-sensors-network)

target_sources(app PRIVATE 
    src/main.c
    sensors/ccs811/ccs811.c
    sensors/sps30/sps30.c
    sensors/sps30/sensirion_common.c
    sensors/sps30/hal.c
    sensors/scd41/scd4x_i2c_example_usage.c
    sensors/scd41/scd4x_i2c.c
    sensors/scd41/sensirion_common.c
    sensors/scd41/sensirion_i2c_hal.c
    sensors/scd41/sensirion_i2c.c
)
