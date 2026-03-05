#ifndef __USB_TASK_H
#define __USB_TASK_H

void vTaskUsb(void *pvParams);

bool is_usb_connected();

#ifdef __cplusplus
extern "C"
#endif
void usbd_serial_init(void);

#endif // __USB_TASK_H
