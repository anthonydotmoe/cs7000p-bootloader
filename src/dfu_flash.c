#include <string.h>

#include "dfu_flash.h"
#include "tusb.h"
#include "stm32h7xx.h"

#define DEBUG_MEASURE 1

#ifdef DEBUG_MEASURE
#include "gpio.h"
#include "pinmap.h"
#endif

// TODO: Error handling

static struct {
    flash_op_state_t state;
    uint32_t addr;
    uint8_t buffer[CFG_TUD_DFU_XFER_BUFSIZE];
    uint16_t length;
    uint16_t offset; // Current write offset
} flash_ctx;

void flash_init(void) {
    flash_ctx.state = FLASH_OP_IDLE;

    // Unlock flash bank 2 if not already
    if ((FLASH->CR2 & FLASH_CR_LOCK) != 0) {
        FLASH->KEYR2 = 0x45670123;
        FLASH->KEYR2 = 0xCDEF89AB;
    }
}

static uint8_t addr_to_sector(uint32_t addr) {
    // Bank 2 at 0x08100000, 128KB sectors
    return ((addr - 0x08100000) / (128 * 1024)) & 0x7;
}

bool flash_erase_sector_async(uint32_t addr) {
    if (flash_ctx.state != FLASH_OP_IDLE) {
        return false; // busy
    }

    flash_ctx.addr = addr;
    flash_ctx.state = FLASH_OP_ERASE_PENDING;
    return true;
}

static bool flash_op_complete(void) {
    bool busy = FLASH->SR2 & FLASH_SR_QW;
    if (busy) return false;

    // If we were programming, clear EOP flag
    if (FLASH->SR2 & FLASH_SR_EOP) {
        FLASH->CCR2 |= FLASH_CCR_CLR_EOP;
    }
    return true;
}

bool flash_write_async(uint32_t addr, const uint8_t *data, uint16_t length) {
    if (flash_ctx.state != FLASH_OP_IDLE) {
        return false; // busy
    }

    // Copy data to internal buffer
    memcpy(flash_ctx.buffer, data, length);
    flash_ctx.addr = addr;
    flash_ctx.length = length;
    flash_ctx.offset = 0;
    flash_ctx.state = FLASH_OP_WRITE_PENDING;

#ifdef DEBUG_MEASURE
    gpio_setPin(PHONE_TXD);
#endif
    return true;
}

void flash_process(void) {
    switch (flash_ctx.state) {
        case FLASH_OP_IDLE:
            // Nothing to do.
            break;
        
        case FLASH_OP_ERASE_PENDING:
            // Wait for any previous operation
            if (!flash_op_complete()) {
                break;
            }

            /* Sector erase sequence */

            // Set programming parallelism
            FLASH->CR2 |= (FLASH_CR_PSIZE_1 | FLASH_CR_PSIZE_0);

            // Sector number
            FLASH->CR2 &= ~FLASH_CR_SNB;
            FLASH->CR2 |= (addr_to_sector(flash_ctx.addr) << FLASH_CR_SNB_Pos);

            // Sector erase command
            FLASH->CR2 |= FLASH_CR_SER;

            // Start
            FLASH->CR2 |= FLASH_CR_START;

            // Command is queued, hand back control
            flash_ctx.state = FLASH_OP_ERASE_BUSY;
            break;

        case FLASH_OP_ERASE_BUSY:
            // Erase command is queued, Check if erase completed
            if (flash_op_complete()) {
                FLASH->CR2 &= ~FLASH_CR_SER;
                flash_ctx.state = FLASH_OP_IDLE;
            }
            break;
        
        case FLASH_OP_WRITE_PENDING:
            // Wait for any previous operation
            if (!flash_op_complete()) {
                break;
            }

            // Enable programming
            FLASH->CR2 |= (FLASH_CR_PSIZE_1 | FLASH_CR_PSIZE_0); // 256-bit
            FLASH->CR2 |= FLASH_CR_PG;

            __ISB();
            __DSB();

            flash_ctx.state = FLASH_OP_WRITE_BUSY;
            __attribute__((fallthrough));
            // Fall through to start first write
        
        case FLASH_OP_WRITE_BUSY:
            // Don't write new data if queue still has pending operations
            if (!flash_op_complete()) {
                break;
            }

            if (flash_ctx.offset < flash_ctx.length) {
                uint32_t remaining = flash_ctx.length - flash_ctx.offset;

                if (remaining >= 32) {
                    // Write next 32-byte chunk (must be 32-byte aligned)
                    uint32_t *src = (uint32_t *)&flash_ctx.buffer[flash_ctx.offset];
                    volatile uint32_t *dst = (volatile uint32_t *)(flash_ctx.addr + flash_ctx.offset);

                    // Write 8 words (32 bytes) - this fills the 256-bit write buffer
                    for (uint8_t i = 0; i < 8; i++) {
                        dst[i] = src[i];
                    }

                    flash_ctx.offset += 32;
                }
                else {
                    // Write remaining bytes (<32)
                    volatile uint8_t *dst = (volatile uint8_t *)(flash_ctx.addr + flash_ctx.offset);
                    uint8_t *src = &flash_ctx.buffer[flash_ctx.offset];

                    for (uint32_t i = 0; i < remaining; i++) {
                        dst[i] = src[i];
                    }

                    flash_ctx.offset = flash_ctx.length;
                }

                __ISB();
                __DSB();
            }
            else {
                // All data written to buffer
                // Check if write buffer has uncommitted data
                if (FLASH->SR2 & FLASH_SR_WBNE) {
                    // Force write the partial buffer
                    FLASH->CR2 |= FLASH_CR_FW;
                    // This will cause QW to go high
                    break; // Wait for force-write to complete
                }

                // No partial data, or force-write complete
                if (flash_op_complete()) {
                    FLASH->CR2 &= ~FLASH_CR_PG;
                    flash_ctx.state = FLASH_OP_IDLE;

#ifdef DEBUG_MEASURE
                    gpio_clearPin(PHONE_TXD);
#endif
                }
            }
            break;
    }
}

bool flash_is_busy(void) {
    return flash_ctx.state != FLASH_OP_IDLE;
}

flash_op_state_t flash_get_state(void) {
    return flash_ctx.state;
}

void flash_erase_sector_blocking(uint32_t addr) {
    flash_erase_sector_async(addr);
    while (flash_is_busy()) {
        flash_process();
    }
}

void flash_write_blocking(uint32_t addr, const uint8_t *data, uint16_t length) {
    flash_write_async(addr, data, length);
    while (flash_is_busy()) {
        flash_process();
    }
}
