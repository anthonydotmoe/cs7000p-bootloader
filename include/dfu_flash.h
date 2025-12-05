#pragma once

#include <stdint.h>
#include <stdbool.h>

#define FLASH_WRITE_SIZE 32 // STM32H7 flash parallelism requirement

typedef enum {
    FLASH_OP_IDLE = 0,
    FLASH_OP_ERASE_PENDING,
    FLASH_OP_ERASE_BUSY,
    FLASH_OP_WRITE_PENDING,
    FLASH_OP_WRITE_BUSY,
} flash_op_state_t;

void flash_init(void);
void flash_process(void);

// Async operations - return immediately
bool flash_erase_sector_async(uint32_t addr);
bool flash_write_async(uint32_t addr, const uint8_t *data, uint16_t length);

// Status checks
bool flash_is_busy(void);
flash_op_state_t flash_get_state(void);

// Blocking operations for simple cases
void flash_erase_sector_blocking(uint32_t addr);
void flash_write_blocking(uint32_t addr, const uint8_t *data, uint16_t length);
