#include <string.h>
#include <stdint.h>

#include "tusb.h"
#include "tusb_config.h"
#include "dfu_flash.h"
#include "debug.h"

#define APP_BASE_ADDR 0x08100000

#define DFUSE_CMD_GET_COMMANDS 0x00
#define DFUSE_CMD_SET_ADDRESS  0x21
#define DFUSE_CMD_ERASE        0x41

static const uint8_t dfuse_cmds[] = { DFUSE_CMD_GET_COMMANDS,
                                      DFUSE_CMD_SET_ADDRESS,
                                      DFUSE_CMD_ERASE };

// DfuSe emulation
typedef enum {
    DFUSE_OP_IDLE = 0,
    DFUSE_OP_ERASE_BUSY,
    DFUSE_OP_WRITE_BUSY,
    DFUSE_OP_SET_ADDR_BUSY,
} dfuse_op_t;

static struct {
    uint32_t   base_addr;     // current DfuSe base address
    bool       have_addr;

    dfuse_op_t op;
    uint32_t   current_addr;  // addr of active erase/write

    bool last_was_get_cmds;   // to answer UPLOAD after 0x00 command
} dfuse_ctx;

// TinyUSB device callbacks

void tud_mount_cb(void) {
    flash_init();

    dfuse_ctx.base_addr         = APP_BASE_ADDR;
    dfuse_ctx.have_addr         = true;
    dfuse_ctx.op                = DFUSE_OP_IDLE;
    dfuse_ctx.current_addr      = 0;
    dfuse_ctx.last_was_get_cmds = false;
}

void tud_umount_cb(void) {

}

// Patched TinyUSB doesn't use this callback anymore, return 0
// The real bwPollTimeout is set via tud_dfu_get_status_cb
uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state) {
    CDC_LOG("get_timeout: alt=%u state=%u\r\n", alt, state);
    return 0;
}

// Helper to write bwPollTimeout in the DFU status response
static void set_poll_timeout(dfu_status_response_t *resp, uint32_t ms) {
    resp->bwPollTimeout[0] = (uint8_t)((ms >>  0) & 0xff);
    resp->bwPollTimeout[1] = (uint8_t)((ms >>  8) & 0xff);
    resp->bwPollTimeout[2] = (uint8_t)((ms >> 16) & 0xff);
}

// DfuSe-style GETSTATUS handling
bool tud_dfu_get_status_cb(uint8_t alt, tud_dfu_get_status_request_t const *req, dfu_status_response_t *resp, tud_dfu_get_status_control_t *ctl) {
    CDC_LOG("get_status_cb: alt=%u state=%u block=%u length=%u\r\n", alt, (unsigned)req->state, req->block, req->length);
    // Default: no extra callbacks. For alt 0/DfuSe we never want tud_dfu_download_cb().
    ctl->invoke_download = false;
    ctl->invoke_manifest = false;

    // DfuSe emulation only on alt 0; let TinyUSB handle other alts normally
    if (alt != 0) {
        return false;
    }

    const dfu_state_t state  = req->state;
    const uint16_t    block  = req->block;
    const uint16_t    length = req->length;
    const uint8_t    *buffer = req->buffer;

    // Start from TinyUSB's current idea of status/state; we'll overwrite.

    // DfuSe GetCommands: DNLOAD block 0, len=1, 0x00, then UPLOAD block 0.
    if (block == 0 && length == 1 && buffer[0] == DFUSE_CMD_GET_COMMANDS) {
        CDC_LOG("  GetCommands\r\n");
        dfuse_ctx.last_was_get_cmds = true;

        resp->bStatus = DFU_STATUS_OK;
        resp->bState  = DFU_DNLOAD_IDLE;
        set_poll_timeout(resp, 0);

        return true;
    }

    // DfuSe SetAddressPointer: DNLOAD block 0, len=5, 0x21, addr bytes
    // Executed when GETSTATUS is processed (AN3156)
    if (block == 0 && length == 5 && buffer[0] == DFUSE_CMD_SET_ADDRESS)  {
        if (state == DFU_DNLOAD_SYNC) {
            uint32_t addr =  (uint32_t)buffer[1]
                          | ((uint32_t)buffer[2] << 8)
                          | ((uint32_t)buffer[3] << 16)
                          | ((uint32_t)buffer[4] << 24);
        
            CDC_LOG("  SetAddress: addr=%08" PRIX32 "\r\n", addr);
            dfuse_ctx.base_addr = addr;
            dfuse_ctx.have_addr = true;
            dfuse_ctx.op        = DFUSE_OP_SET_ADDR_BUSY;

            resp->bStatus = DFU_STATUS_OK;
            resp->bState  = DFU_DNBUSY;
            set_poll_timeout(resp, 0);

            return true;
        }

        // Subsequent GETSTATUS after we already reported DNBUSY
        if (state == DFU_DNBUSY && dfuse_ctx.op == DFUSE_OP_SET_ADDR_BUSY) {
            // We're done
            dfuse_ctx.op = DFUSE_OP_IDLE;

            resp->bStatus = DFU_STATUS_OK;
            resp->bState  = DFU_DNLOAD_IDLE;
            set_poll_timeout(resp, 0);

            return true;
        }
    }

    // DfuSe Erase: DNLOAD block 0, len=5, 0x41, addr bytes
    // Erase starts with first GETSTATUS (state == DFU_DNLOAD_SYNC)
    // Subsequent GETSTATUS poll until flash_is_busy() == false
    if (block == 0 && length >= 1 && buffer[0] == DFUSE_CMD_ERASE)  {
        // First GETSTATUS after DNLOAD: state is DFU_DNLOAD_SYNC
        if (state == DFU_DNLOAD_SYNC) {
            if (length != 5) {
                // mas erase not implemented; report error
                resp->bStatus = DFU_STATUS_ERR_TARGET;
                resp->bState  = DFU_ERROR;
                set_poll_timeout(resp, 0);
                return true;
            }

            uint32_t addr =  (uint32_t)buffer[1]
                          | ((uint32_t)buffer[2] << 8)
                          | ((uint32_t)buffer[3] << 16)
                          | ((uint32_t)buffer[4] << 24);
            CDC_LOG("  EraseSector: addr=%08" PRIX32 "\r\n", addr);
            
            if (!flash_erase_sector_async(addr)) {
                resp->bStatus = DFU_STATUS_ERR_ADDRESS;
                resp->bState  = DFU_ERROR;
                set_poll_timeout(resp, 0);
                return true;
            }

            dfuse_ctx.op           = DFUSE_OP_ERASE_BUSY;
            dfuse_ctx.current_addr = addr;

            resp->bStatus = DFU_STATUS_OK;
            resp->bState  = DFU_DNBUSY;

            // Measured ~844ms; use 900ms as a safe value
            set_poll_timeout(resp, 900);
            
            return true;
        }

        // Subsequent GETSTATUS after we already reported DNBUSY
        if (state == DFU_DNBUSY && dfuse_ctx.op == DFUSE_OP_ERASE_BUSY) {
            if (!flash_is_busy()) {
                // Erase complete
                dfuse_ctx.op = DFUSE_OP_IDLE;

                resp->bStatus = DFU_STATUS_OK;
                resp->bState  = DFU_DNLOAD_IDLE;
                set_poll_timeout(resp, 0);
            } else {
                // Still erasing, ask host to poll again later
                resp->bStatus = DFU_STATUS_OK;
                resp->bState  = DFU_DNBUSY;
                set_poll_timeout(resp, 20);
            }

            return true;
        }
    }

    // Firmware data blocks: DNLOAD block >= 2, len > 0
    // Write starts at first GETSTATUS after DNLOAD
    if (block >= 2 && length > 0 && dfuse_ctx.have_addr) {
        uint32_t addr = dfuse_ctx.base_addr
                      + (uint32_t)(block - 2u) * CFG_TUD_DFU_XFER_BUFSIZE;

        CDC_LOG("  WriteMemory: addr=%08" PRIX32 "\r\n", addr);

        // First GETSTATUS after this DNLOAD: DFU_DNLOAD_SYNC
        if (state == DFU_DNLOAD_SYNC && dfuse_ctx.op == DFUSE_OP_IDLE) {
            if (!flash_write_async(addr, buffer, length)) {
                resp->bStatus = DFU_STATUS_ERR_ADDRESS;
                resp->bState  = DFU_ERROR;
                set_poll_timeout(resp, 0);
            } else {
                dfuse_ctx.op           = DFUSE_OP_WRITE_BUSY;
                dfuse_ctx.current_addr = addr;

                resp->bStatus = DFU_STATUS_OK;
                resp->bState  = DFU_DNBUSY;

                // Measured ~2.7ms
                set_poll_timeout(resp, 3);
            }

            return true;
        }

        // Subsequent GETSTATUS while write is ongoing
        if (state == DFU_DNBUSY && dfuse_ctx.op == DFUSE_OP_WRITE_BUSY) {
            if (!flash_is_busy()) {
                dfuse_ctx.op = DFUSE_OP_IDLE;

                resp->bStatus = DFU_STATUS_OK;
                resp->bState  = DFU_DNLOAD_IDLE;
                set_poll_timeout(resp, 0);
            } else {
                resp->bStatus = DFU_STATUS_OK;
                resp->bState  = DFU_DNBUSY;
                set_poll_timeout(resp, 1);
            }

            return true;
        }
    }

    // Anything else: let TinyUSB's default DFU logic handle it.
    CDC_LOG("  Unhandled by callback\r\n");
    return false;
}

// Upload: used for DfuSe GetCommands and for reading back flash
uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block_num, uint8_t *data, uint16_t length) {
    CDC_LOG("upload_cb: alt=%u block_num=%u length=%u\r\n", alt, block_num, length);
    if (alt != 0) {
        return 0;
    }

    // Block 0 upload after GetCommands: return supported commands.
    if (block_num == 0) {
        if (dfuse_ctx.last_was_get_cmds) {
            CDC_LOG("  GetCmds\r\n");
            uint16_t n = (length < sizeof(dfuse_cmds)) ? length : (uint16_t)sizeof(dfuse_cmds);
            memcpy(data, dfuse_cmds, n);
            dfuse_ctx.last_was_get_cmds = false;
            return n;
        }
        return 0;
    }

    // Read back flash contents
    if (!dfuse_ctx.have_addr || block_num < 2) {
        return 0;
    }

    uint32_t addr = dfuse_ctx.base_addr
                  + (uint32_t)(block_num - 2u) * CFG_TUD_DFU_XFER_BUFSIZE;
    
    volatile uint8_t *src = (volatile uint8_t *)addr;
    for (uint16_t i = 0; i < length; i++) {
        data[i] = src[i];
    }

    return length;
}

// DFU 1.0-type TinyUSB callbacks are unused
void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, uint8_t const *data, uint16_t length) {
    CDC_LOG("download_cb: alt=%u block_num=%u length=%u\r\n", alt, block_num, length);
    (void)data;
    // Everything is done in tud_dfu_get_status_cb()
}
void tud_dfu_manifest_cb(uint8_t alt) {
    CDC_LOG("manifest_cb: alt=%u\r\n", alt);
}

void tud_dfu_abort_cb(uint8_t alt) {
    CDC_LOG("abort_cb: alt=%u\r\n", alt);

    dfuse_ctx.op                = DFUSE_OP_IDLE;
    dfuse_ctx.current_addr      = 0;
    dfuse_ctx.last_was_get_cmds = false;
}

void tud_dfu_detach_cb(void) {
    CDC_LOG("detach_cb\r\n");
}
