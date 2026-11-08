# HP 50g RedEye IR to BLE Keyboard Receiver

An ESP32-C3 receiver that decodes the **HP RedEye** infrared printer protocol from an **HP 50g** calculator and forwards output to a computer or phone as a **Bluetooth Low Energy (BLE) HID Keyboard**.

## Features

- **RedEye IR Decoder**: Decodes 3 start bursts + 12-bit frames (4 Hamming parity bits `H1..H4` + 8 data bits) with automatic 1-bit Hamming error correction and 11-burst erasure recovery.
- **Two Operating Modes**:
  1. **Print Output (`PR1` / `PRSTC` / `PRVAR`)**: Any value, equation, or program printed over IR by the calculator is typed directly via BLE, with HP Roman-8 extended symbols (`«`, `»`, `→`, `≤`, `≥`, `≠`, `π`, `√`, etc.) mapped to ASCII.
  2. **Live Keypad Mode (`TECLA`)**: Using the companion `TECLA` RPL program on the HP 50g, live calculator key presses (`K<RC.P>`) are mapped directly to PC navigation, function, number, and operator keys.
- **Non-Blocking BLE Queue**: Queues outgoing BLE keystrokes and transmits them only while the IR line is idle so BLE radio activity never interferes with IR burst timing.

## Hardware

- **Board**: `esp32-c3-devkitm-1` (ESP32-C3)
- **IR Receiver Pin**: `GPIO 4` (`INPUT_PULLUP`, active-low falling edge)

## Calculator Setup (`TECLA`)

Store the following UserRPL program as `TECLA` on the HP 50g to stream live key presses:

```rpl
« STD -34 CF CLLCD
  "ENVIANDO" 2 DISP
  "PULSACIONES" 3 DISP
  "ON = SALIR" 6 DISP
  DO
    0 WAIT →STR "K" SWAP + PR1 DROP
  UNTIL 0 END
»
```

See [docs/hp50g_keycodes.md](docs/hp50g_keycodes.md) for the full keycode table and PC key mappings.

## Building & Flashing

Built with [PlatformIO](https://platformio.org/):

```bash
pio run -t upload
pio device monitor
```
