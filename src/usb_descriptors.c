#include "tusb.h"

/*
 * Clone ST's VID/PID to be compatible with existing OpenRTX build system
 */
#define USB_VID 0x0483    // ST Microelectronics
#define USB_PID 0xdf11    // STM Device in DFU Mode

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    .bDeviceClass       = TUSB_CLASS_CDC,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

// Invoked when received GET DEVICE DESCRIPTOR
// Application return pointer to descriptor
uint8_t const * tud_descriptor_device_cb(void)
{
    return (uint8_t const *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+
#define ALT_COUNT 1

enum
{
    ITF_NUM_CDC = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_DFU_MODE,
    ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_DFU_DESC_LEN(ALT_COUNT))
#define FUNC_ATTRS        (DFU_ATTR_CAN_UPLOAD | DFU_ATTR_CAN_DOWNLOAD | DFU_ATTR_MANIFESTATION_TOLERANT)

#define EPNUM_CDC_NOTIF 0x81
#define EPNUM_CDC_OUT   0x02
#define EPNUM_CDC_IN    0x82

#define EPNUM_MSC_OUT   0x03
#define EPNUM_MSC_IN    0x83

uint8_t const desc_fs_configuration[] =
{
    // Config number, interface count, string index, total length, attribute,
    // power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

    // Interface number, string index, EP notification address and size, EP data
    // address (out, in) and size.
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8,
                       EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
    
    // Interface number, Alternate count, starting string index, attributes, detach timeout, transfer size
    TUD_DFU_DESCRIPTOR(ITF_NUM_DFU_MODE, ALT_COUNT, 5, FUNC_ATTRS, 1000, CFG_TUD_DFU_XFER_BUFSIZE),
};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void) index;
    return desc_fs_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

static size_t get_unique_id(uint8_t id[], size_t max_len) {
    (void) max_len;
    volatile uint32_t *stm32_uuid = (volatile uint32_t *)UID_BASE;
    uint32_t *id32 = (uint32_t *) (uintptr_t) id;
    uint8_t const len = 12;

    id32[0] = stm32_uuid[0];
    id32[1] = stm32_uuid[1];
    id32[2] = stm32_uuid[2];

    return len;
}

static inline size_t get_serial(uint16_t desc_str1[], size_t max_chars) {
    uint8_t uid[16] TU_ATTR_ALIGNED(4);
    size_t uid_len;

    uid_len = get_unique_id(uid, sizeof(uid));

    if (uid_len > max_chars / 2u) {
        uid_len = max_chars / 2u;
    }

    for (size_t i = 0; i < uid_len; i++) {
        for (size_t j = 0; j < 2; j++) {
            const unsigned char nibble_to_hex[16] = {
                '0', '1', '2', '3', '4', '5', '6', '7',
                '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
            };
            const uint8_t nibble = (uint8_t) ((uid[i] >> (j * 4u)) & 0xFu);
            desc_str1[i * 2 + (1 - j)] = nibble_to_hex[nibble]; // UTF-16-LE
        }
    }

    return 2 * uid_len;
}


enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
};

// array of pointer to string descriptors
static const char* string_desc_arr [] =
{
    (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
    "TinyUSB",                     // 1: Manufacturer
    "TinyUSB Device",              // 2: Product
    "",                            // 3: Serials, should use chip ID
    "TinyUSB CDC",                 // 4: CDC Interface
    "TinyUSB DFU",                 // 5: DFU Interface
};

static uint16_t _desc_str[32 + 1];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long
// enough for transfer to complete
uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void) langid;
    uint8_t chr_count;

    switch (index) {
    case STRID_LANGID:
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
        break;

    case STRID_SERIAL:
        chr_count = get_serial(_desc_str + 1, 32);
        break;
    
    default:
        // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
        // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

        if( !(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) ) { return NULL; }

        const char *str = string_desc_arr[index];

        // Cap at max char
        chr_count = strlen(str);
        size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;
        if (chr_count > max_count) { chr_count = max_count; }

        // Convert ASCII string into UTF-16
        for( size_t i = 0; i < chr_count; i++)
        {
            _desc_str[1+i] = str[i];
        }
        break;
    }

    // first byte is length (including header), second byte is string type
    _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8 ) | (2 * chr_count + 2));
    return _desc_str;
}
