#ifndef __TUSB_CONFIG_H
#define __TUSB_CONFIG_H

#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE

//------------- CLASS -------------//
#define CFG_TUD_CDC                 4
#define CFG_TUD_MSC                 0
#define CFG_TUD_HID                 0
#define CFG_TUD_MIDI                0
#define CFG_TUD_VENDOR              0

// CDC FIFO size of TX and RX, per interface. TX is the larger one: it absorbs UART bursts
// while the host is between polls.
#define CFG_TUD_CDC_RX_BUFSIZE      512
#define CFG_TUD_CDC_TX_BUFSIZE      2048

// CDC Endpoint transfer buffer size, more is faster: one transfer moves this much per
// tud_task() pass, and tud_task() runs once a tick, so 64 bytes would cap a port at 64 KB/s.
#define CFG_TUD_CDC_EP_BUFSIZE      512

//#include <tusb_option.h>

#endif // __TUSB_CONFIG_H
