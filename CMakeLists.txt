# CMakeLists.txt

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(airquality)

# Add source files
target_sources(app PRIVATE src/main.c
src/ccs811.c
src/sps30.c
src/sensirion_common.c
src/hal.c
)
