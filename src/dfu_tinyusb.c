#include <stdint.h>

#include "tusb.h"

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

    tud_dfu_finish_flashing(DFU_STATUS_OK);
}

void tud_dfu_manifest_cb(uint8_t alt) {
    (void)alt;
    tud_dfu_finish_flashing(DFU_STATUS_OK);
}

uint16_t tud_dfu_upload_cb(uint8_t alt, uint16_t block_num, uint8_t *data, uint16_t length) {
    if (block_num != 0u) {
        return 0;
    }

    return length;
}

void tud_dfu_abort_cb(uint8_t alt) {
    (void)alt;
}

void tud_dfu_detach_cb(void) {
    return;
}