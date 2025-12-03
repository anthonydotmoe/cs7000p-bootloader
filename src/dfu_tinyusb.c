#include <stdint.h>

#include "tusb.h"
#include "tusb_config.h"

static uint32_t dfuse_base_addr = 0x08100000;
static bool     dfuse_have_addr = false;
static bool     dfuse_last_was_get_commands = false;
static const uint8_t dfuse_cmds[] = { 0x00, 0x21, 0x41 };

void tud_mount_cb(void) {

}

void tud_umount_cb(void) {

}

uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state) {
    if (state == DFU_DNBUSY) {
        return (alt == 0) ? 1 : 100;
    } else if (state == DFU_MANIFEST) {
        return 0;
    }

    return 0;
}

void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, const uint8_t *data, uint16_t length) {
    (void)alt;
    (void)block_num;

    if (block_num == 0) {
        // DfuSe special commands
        if (length == 1 && data[0] == 0x00) {
            // GetCommands
            dfuse_last_was_get_commands = true;
            tud_dfu_finish_flashing(DFU_STATUS_OK);
            return;
        }

        if (length == 5 && data[0] == 0x21) {
            // Set address pointer
            uint32_t addr =  (uint32_t)data[1]
                          | ((uint32_t)data[2] << 8)
                          | ((uint32_t)data[2] << 16)
                          | ((uint32_t)data[2] << 24);
            dfuse_base_addr = addr;
            dfuse_have_addr = true;
            dfuse_last_was_get_commands = false;
            tud_dfu_finish_flashing(DFU_STATUS_OK);
            return;
        }

        if (length == 5 && data[0] == 0x41) {
            // Erase sector containing address
            uint32_t addr =  (uint32_t)data[1]
                          | ((uint32_t)data[2] << 8)
                          | ((uint32_t)data[2] << 16)
                          | ((uint32_t)data[2] << 24);
            // erase_sector_containing(addr)
            dfuse_last_was_get_commands = false;
            tud_dfu_finish_flashing(DFU_STATUS_OK);
            return;
        }

        // Anything else at block 0
        tud_dfu_finish_flashing(DFU_STATUS_ERR_VENDOR);
        return;
    }

    // Normal data blocks for DfuSe: block_num >= 2
    if (!dfuse_have_addr || block_num < 2) {
        tud_dfu_finish_flashing(DFU_STATUS_ERR_ADDRESS);
        return;
    }

    uint32_t addr = dfuse_base_addr + (block_num - 2) * CFG_TUD_DFU_XFER_BUFSIZE;
    // flash_write(addr, data, length);

    tud_dfu_finish_flashing(DFU_STATUS_OK);
}

void tud_dfu_manifest_cb(uint8_t alt) {
    (void)alt;
    tud_dfu_finish_flashing(DFU_STATUS_OK);
}

uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block_num, uint8_t *data, uint16_t length) {
    (void)alt;

    if (block_num == 0) {
        // Answer GetCommands if host requested it previously
        if (dfuse_last_was_get_commands) {
            uint16_t n = (length < sizeof(dfuse_cmds)) ? length : sizeof(dfuse_cmds);
            memcpy(data, dfuse_cmds, n);
            dfuse_last_was_get_commands = false;
            return n;
        }
        return 0;
    }

    if (!dfuse_have_addr || block_num < 2) {
        return 0;
    }

    uint32_t addr = dfuse_base_addr + (block_num - 2) * CFG_TUD_DFU_XFER_BUFSIZE;
    // flash_read(addr, data, length);
    return length;
}

void tud_dfu_abort_cb(uint8_t alt) {
    (void)alt;
}

void tud_dfu_detach_cb(void) {
    return;
}