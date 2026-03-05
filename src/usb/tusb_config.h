#ifndef __TUSB_CONFIG_H
#define __TUSB_CONFIG_H

#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE

//------------- CLASS -------------//
#define CFG_TUD_CDC                 2
#define CFG_TUD_MSC                 0
#define CFG_TUD_HID                 0
#define CFG_TUD_MIDI                0
#define CFG_TUD_VENDOR              0

// CDC FIFO size of TX and RX
#define CFG_TUD_CDC_RX_BUFSIZE      (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_CDC_TX_BUFSIZE      (TUD_OPT_HIGH_SPEED ? 512 : 64)

// CDC Endpoint transfer buffer size, more is faster
#define CFG_TUD_CDC_EP_BUFSIZE      (TUD_OPT_HIGH_SPEED ? 512 : 64)

//#include <tusb_option.h>

#endif // __TUSB_CONFIG_H
