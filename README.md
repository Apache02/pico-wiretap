# Multi-function USB Bridge for Reverse Engineering on RP2040

## Overview

The device connects to a PC as **4 serial ports** (USB CDC) and provides a set of tools for reverse engineering unknown boards and protocols.

---

## USB CDC Ports

| Port | Name | Purpose |
|------|------|---------|
| CDC0 | `SHELL` | Interactive shell — controls the entire device |
| CDC1 | `UART0` | UART bridge |
| CDC2 | `UART1` | UART bridge |
| CDC3 | `PLOT`  | Text data output, compatible with BetterSerialPlotter |

---

## Functional Modules

### UART Bridge (CDC1, CDC2)

- Two independent UARTs with **automatic configuration** — the device reads the line coding (baud rate, data bits, parity, stop bits) set by the host on the CDC port and mirrors it to the corresponding hardware UART
- Transparent bidirectional data transfer, no user interaction required

### I2C

- **Scanner** — detects all devices on the bus, prints addresses to SHELL
- **Sniffer** — passive bus monitoring, prints transactions to SHELL
- Shell commands: `i2c scan`, `i2c sniff start`, `i2c sniff stop`

---

## PLOT CDC — Streaming Output (CDC3)

Line format compatible with **BetterSerialPlotter**:

```
pwm0:1250 pwm1:980 adc0:2.31 gpio0:1 gpio1:0
```

Each active channel is included in the line. Update rate is configured via SHELL.

### Channel Management via SHELL

```
> plot pwm add 6        # start PWM measurement on pin 6
> plot pwm remove 6
> plot adc add 26       # stream ADC readings from pin 26
> plot gpio add 3       # GPIO monitoring on pin 3 (up to 2 pins)
> plot rate 100         # update interval in ms
> plot status           # show active channels
> plot clear            # stop all channels
```

### PWM Measurement

- Measures **frequency** and **duty cycle**
- Implemented via built-in PWM slice in capture mode or PIO
- **Streaming read** — data is sent to CDC3 `PLOT`

### ADC

- **Single read** via SHELL with output to SHELL: `adc read 26`
- **Streaming read** — data is sent to CDC3 `PLOT`

### GPIO Monitoring

- Reports current state (0/1)
- **Streaming read** — data is sent to CDC3 `PLOT`

---

## Firmware Architecture

**FreeRTOS**, separate tasks:

| Task | Responsibility |
|------|---------------|
| `usb_task` | TinyUSB polling |
| `shell_task` | Read CDC0, parse commands |
| `uart_bridge_task ×2` | Monitor line coding, transparent CDC↔UART forwarding |
| `plot_task` | Collect data from modules, build output line, write to CDC3 |
| `i2c_task` | Scanner / sniffer, output to CDC0 |
| `measure_task` | PWM capture, ADC polling, GPIO polling |

---

## Pin Assignment (approximate)

| Function | Pins |
|----------|------|
| UART0 | GP0, GP1 |
| UART1 | GP4, GP5 |
| I2C (SDA/SCL) | GP2, GP3 |
| PWM capture | GP6, GP7, GP12, GP13 |
| ADC | GP26, GP27, GP28 |
| GPIO monitor | GP14, GP15 |
