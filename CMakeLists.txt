# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(embedded_rtos_sensor_hub)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
  src/main.c
  src/sensor_hub.c
)
