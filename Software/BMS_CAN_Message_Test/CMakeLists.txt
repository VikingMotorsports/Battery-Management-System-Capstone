cmake_minimum_required(VERSION 3.20.0)


find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(CAN)

target_sources(app PRIVATE
    src/main.c
    src/monitor/init/init.c
    src/monitor/calculations/calculations.c
)

target_include_directories(app PRIVATE
    src
    src/monitor
    src/monitor/init
    src/monitor/calculations
)
