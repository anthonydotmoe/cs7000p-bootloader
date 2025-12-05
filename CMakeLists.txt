cmake_minimum_required(VERSION 3.23)

project(cs7000p_bootloader C ASM)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m7)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS)

# Paths ------------------------------------------------------------------------

set (PROJECT_SRC_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/src")
set (PROJECT_INC_DIR     "${CMAKE_CURRENT_SOURCE_DIR}/include")
set (PROJECT_LINKER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/linker/bootloader.ld")

# Libraries --------------------------------------------------------------------

# STM32 H7 CMSIS
set(CMSIS_H7_ROOT       ${CMAKE_CURRENT_LIST_DIR}/lib/STM32CubeH7)
set(CMSIS_H7_DEVICE_DIR ${CMSIS_H7_ROOT}/Drivers/CMSIS/Device/ST/STM32H7xx)
set(CMSIS_H7_CORE_DIR   ${CMSIS_H7_ROOT}/Drivers/CMSIS/Include)
set(CMSIS_H7_DEVICE_DEFINE STM32H743xx)

add_library(cmsis_h7_headers INTERFACE)

target_include_directories(cmsis_h7_headers
    INTERFACE
        ${CMSIS_H7_CORE_DIR}
        ${CMSIS_H7_DEVICE_DIR}/Include
)

# TinyUSB
set(TINYUSB_ROOT ${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb)
set(TINYUSB_CORE_SOURCES
    ${TINYUSB_ROOT}/src/tusb.c
    ${TINYUSB_ROOT}/src/common/tusb_fifo.c
    ${TINYUSB_ROOT}/src/device/usbd.c
    ${TINYUSB_ROOT}/src/device/usbd_control.c
)
set(TINYUSB_CLASS_SOURCES
    ${TINYUSB_ROOT}/src/class/cdc/cdc_device.c
    ${TINYUSB_ROOT}/src/class/dfu/dfu_device.c
)
set(TINYUSB_PORT_SOURCES
    ${TINYUSB_ROOT}/src/portable/synopsys/dwc2/dcd_dwc2.c
    ${TINYUSB_ROOT}/src/portable/synopsys/dwc2/dwc2_common.c
)

set(TUSB_SRC
    ${TINYUSB_CORE_SOURCES}
    ${TINYUSB_CLASS_SOURCES}
    ${TINYUSB_PORT_SOURCES}
)

# Source files -----------------------------------------------------------------

set(SRC_FILES
    ${PROJECT_SRC_DIR}/boot_jump.c
    ${PROJECT_SRC_DIR}/clock.c
    ${PROJECT_SRC_DIR}/debug.c
    ${PROJECT_SRC_DIR}/delay.c
    ${PROJECT_SRC_DIR}/dfu_tinyusb.c
    ${PROJECT_SRC_DIR}/dfu_flash.c
    ${PROJECT_SRC_DIR}/gpio.c
    ${PROJECT_SRC_DIR}/init.c
    ${PROJECT_SRC_DIR}/main.c
    ${PROJECT_SRC_DIR}/startup.c
    ${PROJECT_SRC_DIR}/syscalls.c
    ${PROJECT_SRC_DIR}/usb_descriptors.c
    ${CMSIS_H7_DEVICE_DIR}/Source/Templates/system_stm32h7xx.c
    ${TUSB_SRC}
)

add_executable(${PROJECT_NAME} ${SRC_FILES})

# Include paths ----------------------------------------------------------------

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_INC_DIR}
        ${TINYUSB_ROOT}/src
)

# Compiler / assembler / linker flags ------------------------------------------

# Common flags for C/ASM
set(MCU_FLAGS
    -mcpu=cortex-m7
    -mthumb
    -mfpu=fpv5-d16
    -mfloat-abi=hard
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        ${CMSIS_H7_DEVICE_DEFINE}
)

# C flags
target_compile_options(${PROJECT_NAME}
    PRIVATE
        ${MCU_FLAGS}
        -ffreestanding
        -fdata-sections
        -ffunction-sections
        -Wall
        -Wextra
        -Wundef
        -Wno-unused-parameter
        $<$<CONFIG:Debug>:-O0 -g3>
        $<$<CONFIG:Release>:-O2>
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
)

# Assembler flags
set_property(SOURCE ${PROJECT_SRC_DIR}/startup_stm32h743xx.s
    APPEND_STRING PROPERTY COMPILE_FLAGS " ${MCU_FLAGS}"
)

# Linker flags
target_link_options(${PROJECT_NAME}
    PRIVATE
        ${MCU_FLAGS}
        -T${PROJECT_LINKER_FILE}
        -Wl,-Map=${PROJECT_NAME}.map
        -Wl,--gc-sections
        -Wl,--print-memory-usage
)

# No standard system libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        cmsis_h7_headers
)

# Post-build: create .bin ------------------------------------------------------

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary
        $<TARGET_FILE:${PROJECT_NAME}>
        ${PROJECT_NAME}.bin
    COMMENT "Generating BIN file"
)
