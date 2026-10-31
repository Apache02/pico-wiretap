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

// CDC Endpoint transfer buffer size. Must stay one packet: an OUT transfer only completes
// when this many bytes or a short packet arrive, and Linux cdc-acm sends no ZLP, so a
// stream ending on a full packet below this size would sit in the endpoint unseen.
// Throughput comes from the bridge tasks flushing on every wake, not from a larger buffer.
#define CFG_TUD_CDC_EP_BUFSIZE      64

//#include <tusb_option.h>

#endif // __TUSB_CONFIG_H
