# SPDX-License-Identifier: Apache-2.0

# # Manually add the arducam driver to the example
# list(APPEND ZEPHYR_EXTRA_MODULES
#   ${CMAKE_CURRENT_SOURCE_DIR}/../../driver/arducam
#   )

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(simple)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../drivers/arducam ${CMAKE_CURRENT_BINARY_DIR}/arducam)

target_include_directories(app PRIVATE src)
target_sources(app PRIVATE src/main.c)