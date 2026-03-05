#include "tusb.h"
#include "pico/unique_id.h"


#define DESC_STR_MAX 20

#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )

#define USBD_VID 0x2E8A /* Raspberry Pi */
#define USBD_PID (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 2) | _PID_MAP(HID, 4) | \
                           _PID_MAP(MIDI, 6) | _PID_MAP(VENDOR, 8) )

#define USBD_DESC_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN * CFG_TUD_CDC)
#define USBD_MAX_POWER_MA 500

#define USBD_CDC_0_EP_CMD 0x81
#define USBD_CDC_0_EP_IN 0x82
#define USBD_CDC_0_EP_OUT 0x01

#define USBD_CDC_1_EP_CMD 0x83
#define USBD_CDC_1_EP_IN 0x84
#define USBD_CDC_1_EP_OUT 0x03

#define USBD_CDC_CMD_MAX_SIZE 8
#define USBD_CDC_IN_OUT_MAX_SIZE 64

#define USBD_STR_SERIAL_LEN 17
#define USBD_STR_LANGUAGE (0x00)
#define USBD_STR_MANUF (0x01)
#define USBD_STR_PRODUCT (0x02)
#define USBD_STR_SERIAL (0x03)
#define USBD_STR_CDC (0x04)

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+

static const tusb_desc_device_t desc_device = {
        .bLength            = sizeof(tusb_desc_device_t),
        .bDescriptorType    = TUSB_DESC_DEVICE,
        .bcdUSB             = 0x0200,
        .bDeviceClass       = TUSB_CLASS_MISC,
        .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
        .bDeviceProtocol    = MISC_PROTOCOL_IAD,
        .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
        .idVendor           = USBD_VID,
        .idProduct          = USBD_PID,
        .bcdDevice          = 0x0100,
        .iManufacturer      = USBD_STR_MANUF,
        .iProduct           = USBD_STR_PRODUCT,
        .iSerialNumber      = USBD_STR_SERIAL,
        .bNumConfigurations = 1,
};

const uint8_t *tud_descriptor_device_cb(void) {
    return (const uint8_t *) &desc_device;
}

//--------------------------------------------------------------------+
// Configuration Descriptor
//--------------------------------------------------------------------+

enum
{
    ITF_NUM_CDC_0 = 0,
    ITF_NUM_CDC_0_DATA,
    ITF_NUM_CDC_1,
    ITF_NUM_CDC_1_DATA,
    ITF_NUM_TOTAL
};

static const uint8_t desc_config[USBD_DESC_LEN] = {
        TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, USBD_STR_LANGUAGE, USBD_DESC_LEN,
                              TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, USBD_MAX_POWER_MA),

        TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_0, USBD_STR_CDC, USBD_CDC_0_EP_CMD,
                           USBD_CDC_CMD_MAX_SIZE, USBD_CDC_0_EP_OUT, USBD_CDC_0_EP_IN,
                           USBD_CDC_IN_OUT_MAX_SIZE),

        TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_1, USBD_STR_CDC, USBD_CDC_1_EP_CMD,
                           USBD_CDC_CMD_MAX_SIZE, USBD_CDC_1_EP_OUT, USBD_CDC_1_EP_IN,
                           USBD_CDC_IN_OUT_MAX_SIZE),
};

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    return desc_config;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

static char usb_serial[USBD_STR_SERIAL_LEN] = "000000000000";

void usbd_serial_init(void) {
    pico_get_unique_board_id_string(usb_serial, sizeof(usb_serial));
}

static const char *const desc_string_arr[] = {
        [USBD_STR_LANGUAGE] = (const char[]) {0x09, 0x04},  // 0: is supported language is English (0x0409)
        [USBD_STR_MANUF] = "Raspberry Pi",                  // 1: Manufacturer
        [USBD_STR_PRODUCT] = "Pico Shell",                // 2: Product
        [USBD_STR_SERIAL] = usb_serial,                     // 3: Serials, should use chip ID
        [USBD_STR_CDC] = "CDC",                             // 4: CDC Interface
};

static uint16_t desc_str[DESC_STR_MAX];

const uint16_t *tud_descriptor_string_cb(uint8_t index, __unused uint16_t langid) {
    uint8_t len;

    if (index == USBD_STR_LANGUAGE) {
        desc_str[1] = 0x0409;
        len = 1;
    } else {
        if (index >= count_of(desc_string_arr)) {
            return NULL;
        }

        const char *src = desc_string_arr[index];
        for (len = 0; len < DESC_STR_MAX - 1 && src[len]; ++len) {
            desc_str[1 + len] = src[len];
        }
    }

    desc_str[0] = (TUSB_DESC_STRING << 8) | ((len + 1) * 2);

    return desc_str;
}

