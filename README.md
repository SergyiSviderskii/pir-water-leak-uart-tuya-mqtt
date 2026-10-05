# pir-water-leak-uart-tuya-mqtt

Universal ESPHome C++ custom component for **TuyaMCU v0** battery-powered water leak and PIR sensors.  
Supports **ESP8266** (`TYWE3S`) and **BK7231N** (`CBU`) via [LibreTiny](https://github.com/libretiny-eu/libretiny).  
Fixes Beken bootloader serial noise and grouped frame buffer truncation — two critical issues that break UART communication on battery-powered Tuya devices.


## 📋 Table of Contents

- [Features](#features)
- [Tested Devices](#tested-devices)
- [ESPHome Version Compatibility](#-esphome-version-compatibility)
- [Environment & Build Target](#-environment--build-target)
- [First-Time Flashing (By Wire)](#-first-time-flashing-flashing-by-wire)
- [Device Reset & OTA Firmware Update Logic](#-device-reset--ota-firmware-update-logic)
- [Device-Specific Notes](#-device-specific-notes)
- [Repository Structure](#-repository-structure)
- [How It Works](#how-it-works)
- [Troubleshooting](#troubleshooting)
- [License](#license)

---

## Features

- **UART protocol repair** — reconstructs TuyaMCU data frames that arrive fragmented or concatenated due to the low-power wakeup cycle
- **Beken bootloader noise filter** — strips garbage bytes injected by the BK7231N bootloader on cold boot, which corrupt the first UART frames
- **Port noise suppression** — cleans GPIO lines and clears serial buffer garbage during `setup()` before UART communication begins
- **Battery-compatible** — designed for devices where the Wi-Fi module is fully unpowered between events (4–5s active window)
- **OTA support** — wireless firmware updates via physical button override (no disassembly needed after initial flash)
- **MQTT-ready** — integrates with Home Assistant via MQTT or direct ESPHome API
- **Universal codebase** — one component, three devices, shared `.cpp` / `.h` files

---

## Tested Devices

| Device | MCU | Wi-Fi Module | Protocol | Battery Monitoring |
|--------|-----|-------------|----------|-------------------|
| Tuya water leak sensor (battery) | TuyaMCU v0 | TYWE3S (ESP8266) | UART 9600 8N1 | Voltage divider (internal ADC) |
| Tuya PIR motion sensor (battery) | TuyaMCU v0 | TYWE3S (ESP8266) | UART 9600 8N1 | TuyaMCU DP report |
| Tuya PIR motion sensor (battery) | TuyaMCU v0 | CBU (BK7231N) | UART 9600 8N1 | TuyaMCU DP report |


> **Note:** Any TuyaMCU v0 battery device using the `55 AA` frame protocol over UART at 9600 baud should be compatible. Untested devices may require minor DP-ID adjustments in the configuration.

---

## 📟 ESPHome Version Compatibility

All versions below have been **personally tested** with this component:

| ESPHome Version | Status | Notes |
|---|---|---|
|  2026.5.3 | ✅ Works | Stable |
| 2026.6.0 — 2026.7.4 | ✅ **Recommended** | Best stability |
| 2026.8.x | ✅ Stable | No issues observed |
| 2026.9.0+ | ❌ Breaks code | Do not use |

---

## 🔧 Environment & Build Target

This component has been verified and compiled using the production release of ESPHome:

- **ESPHome Version:** `2026.6.6` (recommended)
- **Configuration Hash:** `0xa44d4e42`
- **Build Timestamp:** `2026-09-21 21:42:27 +0300`

---

## ⚡ First-Time Flashing (Flashing "By Wire")

Battery-powered devices keep the Wi-Fi module completely powered down until a physical sensor event or button press occurs. Performing the initial firmware installation requires temporary hardware modifications:

1. **Isolate the MCU:** Disconnect or desolder the `TX` and `RX` lines between the TuyaMCU and the Wi-Fi module (`TYWE3S` or `CBU`) to prevent serial bus contention during programming.
2. **Apply Stable External Power:** Provide a solid, continuous `3.3V` power supply directly to the Wi-Fi chip from your USB-to-UART adapter (ensure common ground `GND`). Do not rely on the device's battery lines during this stage.
3. **Flash the Binary:** Use your standard serial flashing utility:
   - **ESP8266:** `esptool.py --port /dev/ttyUSB0 write_flash 0x0 firmware.bin or ESPHOME
   - **BK7231N:** `ltchiptool flash firmware.bin` or the [LibreTiny flasher](https://github.com/libretiny-eu/libretiny)
4. **Restore the Bus:** Once successfully flashed, reconnect the UART lines back to the TuyaMCU.

---

## 🔘 Device Reset & OTA Firmware Update Logic

Once the device is assembled and running on battery power, it is impossible to flash it over-the-air (OTA) under normal operation because the chip cuts its own power line within **4–5 seconds**.

To bypass this and push updates wirelessly, use the **physical reset / pairing button**:

1. **Trigger the timeout loop:** Press and hold the physical button inside the device until the status LEDs begin to blink rapidly.
2. **MCU hold-on state:** The TuyaMCU detects this hardware override sequence and enters an autoconfiguration / pairing mode. Instead of shutting down after 4 seconds, the MCU will force-keep the Wi-Fi chip powered on for approximately **60–180 seconds**, waiting for a network handshake.
3. **Executing OTA:** Within this window, your automation scripts must initiate the wireless ESPHome OTA update process.
4. **Automatic reboot:** If the OTA fails or times out, the device will automatically reboot after ~60 seconds, cut the power rail, and return to its ultra-low-power event-driven sleep routine.

### Button Sequence

- **First press** → OTA mode (LEDs blinking) — flash wirelessly within ~1 - 3 minute
- **Second press** → full device reset -> leak-water

### UART Debug Output (OTA Window)

```
INFO ESPHome 2026.9.0
INFO Loaded validated config cache for leak-water-house-sum-cuisi-sink.yaml, skipping validation.
INFO Starting log output from leak-water-house-sum-cuisi-sink/debug
INFO Connected to MQTT broker!
[22:48:04][E][main:194]: ===============================
[22:48:04][E][main:195]: === ХРОНОЛОГИЯ ОБМЕНА С MCU === 4597
[22:48:04][E][main:215]:   [Sending] -----> [55 AA 00 00 00 00 FF ] Время: 123 мс
[22:48:04][E][main:215]:   [Sending] -----> [55 AA 00 01 00 00 00 ] Время: 430 мс
[22:48:04][E][main:215]:   [Received] <----- [55 AA 00 01 00 24 7B 22 70 22 3A 22 64 73 6D 6A 75 66 65 6D 7A 6F 63 63 33 33 6F 30 22 2C 22 76 22 3A 22 31 2E 30 2E 30 22 7D AE ] Время: 480 мс
[22:48:04][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 02 04 ] Время: 489 мс
[22:48:04][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 505 мс
[22:48:04][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 03 05 ] Время: 514 мс
[22:48:04][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 531 мс
[22:48:04][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 04 06 ] Время: 539 мс
[22:48:04][E][main:215]:   [Received] <----- [55 AA 00 05 00 05 01 04 00 01 00 0F ] Время: 555 мс
[22:48:04][E][main:215]:   [Received] <----- [55 AA 00 05 00 05 03 04 00 01 02 13 ] Время: 572 мс
[22:48:04][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 572 мс
[22:48:04][E][main:215]:   [Sending] -----> [55 AA 00 05 00 01 00 05 ] Время: 601 мс
[22:48:04][E][main:224]: === Приватный буфер успешно очищен ===
[22:48:04][E][main:230]: ===============================
[22:48:05][E][tuya:364]:    [Sending] -----> [55.AA.00.05.00.01.00.05 (8)] Время: 4883 мс
[22:48:16][E][main:194]: ===============================
[22:48:16][E][main:195]: === ХРОНОЛОГИЯ ОБМЕНА С MCU === 4638
[22:48:16][E][main:215]:   [Sending] -----> [55 AA 00 00 00 00 FF ] Время: 124 мс
[22:48:16][E][main:215]:   [Sending] -----> [55 AA 00 01 00 00 00 ] Время: 430 мс
[22:48:16][E][main:215]:   [Received] <----- [55 AA 00 01 00 24 7B 22 70 22 3A 22 64 73 6D 6A 75 66 65 6D 7A 6F 63 63 33 33 6F 30 22 2C 22 76 22 3A 22 31 2E 30 2E 30 22 7D AE ] Время: 481 мс
[22:48:16][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 02 04 ] Время: 490 мс
[22:48:16][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 506 мс
[22:48:16][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 03 05 ] Время: 515 мс
[22:48:16][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 532 мс
[22:48:16][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 04 06 ] Время: 541 мс
[22:48:16][E][main:215]:   [Received] <----- [55 AA 00 05 00 05 01 04 00 01 01 10 ] Время: 557 мс
[22:48:16][E][main:215]:   [Received] <----- [55 AA 00 05 00 05 03 04 00 01 02 13 ] Время: 574 мс
[22:48:16][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 574 мс
[22:48:16][E][main:215]:   [Sending] -----> [55 AA 00 05 00 01 00 05 ] Время: 603 мс
[22:48:16][E][main:224]: === Приватный буфер успешно очищен ===
[22:48:16][E][main:230]: ===============================
[22:48:16][E][tuya:364]:    [Sending] -----> [55.AA.00.05.00.01.00.05 (8)] Время: 4923 мс
[22:48:33][E][main:194]: ===============================
[22:48:33][E][main:195]: === ХРОНОЛОГИЯ ОБМЕНА С MCU === 4627
[22:48:33][E][main:215]:   [Sending] -----> [55 AA 00 00 00 00 FF ] Время: 122 мс
[22:48:33][E][main:215]:   [Sending] -----> [55 AA 00 01 00 00 00 ] Время: 431 мс
[22:48:33][E][main:215]:   [Received] <----- [55 AA 00 01 00 24 7B 22 70 22 3A 22 64 73 6D 6A 75 66 65 6D 7A 6F 63 63 33 33 6F 30 22 2C 22 76 22 3A 22 31 2E 30 2E 30 22 7D AE ] Время: 481 мс
[22:48:33][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 02 04 ] Время: 490 мс
[22:48:33][E][main:215]:   [Received] <----- [55 AA 00 07 00 00 06 ] Время: 506 мс
[22:48:33][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 02 04 ] Время: 806 мс
[22:48:33][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 823 мс
[22:48:33][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 03 05 ] Время: 832 мс
[22:48:33][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 848 мс
[22:48:33][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 04 06 ] Время: 857 мс
[22:48:33][E][main:215]:   [Received] <----- [55 AA 00 05 00 05 01 04 00 01 01 10 ] Время: 874 мс
[22:48:33][E][main:215]:   [Received] <----- [55 AA 00 05 00 05 03 04 00 01 02 13 ] Время: 891 мс
[22:48:33][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 891 мс
[22:48:33][E][main:215]:   [Sending] -----> [55 AA 00 05 00 01 00 05 ] Время: 920 мс
[22:48:33][E][main:224]: === Приватный буфер успешно очищен ===
[22:48:33][E][main:230]: ===============================
[22:48:33][E][tuya:364]:    [Sending] -----> [55.AA.00.05.00.01.00.05 (8)] Время: 4915 мс
[22:48:33][E][tuya:220]: =========WIFI OTA=========
[22:48:33][E][tuya:221]: [Received] <-----:FRAME=[55.AA.00.04.00.01.00.04 (8)] 5255  
[22:48:33][E][tuya:364]:    [Sending] -----> [55.AA.00.05.00.01.00.05 (8)] Время: 5256 мс
[22:48:55][E][tuya:364]:    [Sending] -----> [55.AA.00.05.00.01.00.05 (8)] Время: 27413 мс
[22:48:56][E][tuya:364]:    [Sending] -----> [55.AA.00.05.00.01.00.05 (8)] Время: 27718 мс
[22:48:56][E][tuya:364]:    [Sending] -----> [55.AA.00.05.00.01.00.05 (8)] Время: 28022 мс
[22:48:56][E][tuya:203]: =========RESET=========
[22:48:59][E][main:194]: ===============================
[22:48:59][E][main:195]: === ХРОНОЛОГИЯ ОБМЕНА С MCU === 2601
[22:48:59][E][main:215]:   [Sending] -----> [55 AA 00 00 00 00 FF ] Время: 126 мс
[22:48:59][E][main:215]:   [Sending] -----> [55 AA 00 01 00 00 00 ] Время: 1158 мс
[22:48:59][E][main:215]:   [Received] <----- [55 AA 00 01 00 24 7B 22 70 22 3A 22 64 73 6D 6A 75 66 65 6D 7A 6F 63 63 33 33 6F 30 22 2C 22 76 22 3A 22 31 2E 30 2E 30 22 7D AE ] Время: 1209 мс
[22:48:59][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 02 04 ] Время: 1218 мс
[22:48:59][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 1234 мс
[22:48:59][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 03 05 ] Время: 1243 мс
[22:48:59][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 1259 мс
[22:48:59][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 04 06 ] Время: 1268 мс
[22:48:59][E][main:215]:   [Received] <----- [55 AA 00 05 00 05 01 04 00 01 01 10 ] Время: 1286 мс
[22:48:59][E][main:215]:   [Received] <----- [55 AA 00 05 00 05 03 04 00 01 02 13 ] Время: 1305 мс
[22:48:59][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 1305 мс
[22:48:59][E][main:215]:   [Sending] -----> [55 AA 00 05 00 01 00 05 ] Время: 1334 мс
[22:48:59][E][main:224]: === Приватный буфер успешно очищен ===
[22:48:59][E][main:230]: ===============================
[22:48:59][E][tuya:364]:    [Sending] -----> [55.AA.00.05.00.01.00.05 (8)] Время: 2885 мс


INFO ESPHome 2026.9.0
INFO Loaded validated config cache for pir-cbu-module-bk7231n.yaml, skipping validation.
INFO Starting log output from pir-house-test-beken/debug
INFO Connected to MQTT broker!
[22:52:43][E][main:194]: ===============================
[22:52:43][E][main:195]: === ХРОНОЛОГИЯ ОБМЕНА С MCU === 5532
[22:52:43][E][main:215]:   [Sending] -----> [55 AA 00 00 00 00 FF ] Время: 188 мс
[22:52:43][E][main:215]:   [Sending] -----> [55 AA 00 01 00 00 00 ] Время: 624 мс
[22:52:43][E][main:215]:   [Received] <----- [55 AA 00 01 00 24 7B 22 70 22 3A 22 32 73 37 77 77 62 63 6B 66 35 78 37 77 73 7A 68 22 2C 22 76 22 3A 22 31 2E 30 2E 30 22 7D AF ] Время: 642 мс
[22:52:43][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 02 04 ] Время: 644 мс
[22:52:43][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 662 мс
[22:52:43][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 03 05 ] Время: 662 мс
[22:52:43][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 680 мс
[22:52:43][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 04 06 ] Время: 680 мс
[22:52:43][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 698 мс
[22:52:43][E][main:215]:   [Received] <----- [55 AA 00 05 00 0D 01 04 00 01 01 04 02 00 04 00 00 00 64 86 ] Время: 698 мс
[22:52:43][E][main:215]:   [Sending] -----> [55 AA 00 05 00 01 00 05 ] Время: 718 мс
[22:52:43][E][main:215]:   [Received] <----- [55 AA 00 10 00 01 00 10 ] Время: 736 мс
[22:52:43][E][main:215]:   [Sending] -----> [55 AA 00 10 00 00 0F ] Время: 1006 мс
[22:52:43][E][main:215]:   [Received] <----- [55 AA 00 05 00 0A 09 04 00 01 01 0A 04 00 01 00 2C ] Время: 1024 мс
[22:52:43][E][main:215]:   [Received] <----- [55 AA 00 05 00 0D 01 04 00 01 00 04 02 00 04 00 00 00 64 85 ] Время: 2628 мс
[22:52:43][E][main:215]:   [Received] <----- [55 AA 00 05 00 0D 01 04 00 01 00 04 02 00 04 00 00 00 64 85 ] Время: 4634 мс
[22:52:43][E][main:224]: === Приватный буфер успешно очищен ===
[22:52:43][E][main:230]: ===============================
[22:52:43][E][tuya:364]:    [Sending] -----> [55.AA.00.05.00.01.00.05 (8)] Время: 5834 мс
[22:52:44][E][tuya:364]:    [Sending] -----> [55.AA.00.10.00.00.0F (7)] Время: 6146 мс
[22:53:15][E][main:194]: ===============================
[22:53:15][E][main:195]: === ХРОНОЛОГИЯ ОБМЕНА С MCU === 5544
[22:53:15][E][main:215]:   [Sending] -----> [55 AA 00 00 00 00 FF ] Время: 188 мс
[22:53:15][E][main:215]:   [Sending] -----> [55 AA 00 01 00 00 00 ] Время: 624 мс
[22:53:15][E][main:215]:   [Received] <----- [55 AA 00 01 00 24 7B 22 70 22 3A 22 32 73 37 77 77 62 63 6B 66 35 78 37 77 73 7A 68 22 2C 22 76 22 3A 22 31 2E 30 2E 30 22 7D AF ] Время: 642 мс
[22:53:15][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 02 04 ] Время: 644 мс
[22:53:15][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 662 мс
[22:53:15][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 03 05 ] Время: 662 мс
[22:53:15][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 680 мс
[22:53:15][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 04 06 ] Время: 680 мс
[22:53:15][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 698 мс
[22:53:15][E][main:215]:   [Received] <----- [55 AA 00 05 00 0D 01 04 00 01 00 04 02 00 04 00 00 00 64 85 ] Время: 698 мс
[22:53:15][E][main:215]:   [Sending] -----> [55 AA 00 05 00 01 00 05 ] Время: 718 мс
[22:53:15][E][main:215]:   [Received] <----- [55 AA 00 10 00 01 00 10 ] Время: 736 мс
[22:53:15][E][main:215]:   [Sending] -----> [55 AA 00 10 00 00 0F ] Время: 1006 мс
[22:53:15][E][main:215]:   [Received] <----- [55 AA 00 05 00 0A 09 04 00 01 01 0A 04 00 01 00 2C ] Время: 1024 мс
[22:53:15][E][main:215]:   [Received] <----- [55 AA 00 05 00 0A 09 04 00 01 01 0A 04 00 01 00 2C ] Время: 3024 мс
[22:53:15][E][main:215]:   [Received] <----- [55 AA 00 03 00 00 02 ] Время: 4854 мс
[22:53:15][E][main:215]:   [Sending] -----> [55 AA 00 03 00 01 02 05 ] Время: 4856 мс
[22:53:15][E][main:224]: === Приватный буфер успешно очищен ===
[22:53:15][E][main:230]: ===============================
[22:53:15][E][tuya:364]:    [Sending] -----> [55.AA.00.05.00.01.00.05 (8)] Время: 5850 мс
[22:53:35][E][tuya:364]:    [Sending] -----> [55.AA.00.03.00.01.02.05 (8)] Время: 25072 мс



INFO ESPHome 2026.9.0
INFO Loaded validated config cache for pir-tywe3s-module-esp8266.yaml, skipping validation.
INFO Starting log output from pir-tuywe3s-module-esp8266/debug
INFO Connected to MQTT broker!
[22:56:27][E][main:194]: ===============================
[22:56:27][E][main:195]: === ХРОНОЛОГИЯ ОБМЕНА С MCU === 4654
[22:56:27][E][main:215]:   [Sending] -----> [55 AA 00 00 00 00 FF ] Время: 129 мс
[22:56:27][E][main:215]:   [Received] <----- [55 AA 00 00 00 01 01 01 ] Время: 151 мс
[22:56:27][E][main:215]:   [Sending] -----> [55 AA 00 01 00 00 00 ] Время: 159 мс
[22:56:27][E][main:215]:   [Received] <----- [55 AA 00 01 00 24 7B 22 70 22 3A 22 7A 64 66 75 6B 7A 6F 62 36 6A 39 6E 32 6C 71 76 22 2C 22 76 22 3A 22 31 2E 30 2E 30 22 7D DA ] Время: 210 мс
[22:56:27][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 02 04 ] Время: 219 мс
[22:56:27][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 235 мс
[22:56:27][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 03 05 ] Время: 245 мс
[22:56:27][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 261 мс
[22:56:27][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 04 06 ] Время: 270 мс
[22:56:27][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 287 мс
[22:56:27][E][main:215]:   [Received] <----- [55 AA 00 05 00 05 01 04 00 01 00 0F ] Время: 303 мс
[22:56:27][E][main:215]:   [Sending] -----> [55 AA 00 05 00 01 00 05 ] Время: 333 мс
[22:56:27][E][main:215]:   [Received] <----- [55 AA 00 05 00 05 03 04 00 01 02 13 ] Время: 350 мс
[22:56:27][E][main:224]: === Приватный буфер успешно очищен ===
[22:56:27][E][main:230]: ===============================
[22:56:42][E][main:194]: ===============================
[22:56:42][E][main:195]: === ХРОНОЛОГИЯ ОБМЕНА С MCU === 4654
[22:56:42][E][main:215]:   [Sending] -----> [55 AA 00 00 00 00 FF ] Время: 128 мс
[22:56:42][E][main:215]:   [Received] <----- [55 AA 00 00 00 01 01 01 ] Время: 150 мс
[22:56:42][E][main:215]:   [Sending] -----> [55 AA 00 01 00 00 00 ] Время: 159 мс
[22:56:42][E][main:215]:   [Received] <----- [55 AA 00 01 00 24 7B 22 70 22 3A 22 7A 64 66 75 6B 7A 6F 62 36 6A 39 6E 32 6C 71 76 22 2C 22 76 22 3A 22 31 2E 30 2E 30 22 7D DA ] Время: 209 мс
[22:56:42][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 02 04 ] Время: 218 мс
[22:56:42][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 235 мс
[22:56:42][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 03 05 ] Время: 244 мс
[22:56:42][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 260 мс
[22:56:42][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 04 06 ] Время: 269 мс
[22:56:42][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 286 мс
[22:56:42][E][main:224]: === Приватный буфер успешно очищен ===
[22:56:42][E][main:230]: ===============================
[22:56:42][E][tuya:220]: =========WIFI OTA=========
[22:56:42][E][tuya:221]: [Received] <-----:FRAME=[55.AA.00.04.00.01.00.04 (8)] 4874  
[22:56:42][E][tuya:364]:    [Sending] -----> [55.AA.00.05.00.01.00.05 (8)] Время: 4875 мс
```

---

## 🔩 Device-Specific Notes

### PIR Motion Sensor (ESP8266 / TYWE3S)

- **Static IP is mandatory.** The firmware uses static IP addresses — this is the most reliable approach for battery-powered devices where DHCP lease times are unpredictable.
- **OTA procedure:** Press and hold the reset button → LEDs start blinking → OTA window opens for ~1 minute. Flash wirelessly within this window.
- **MQTT hostname over IP:** In the MQTT configuration, use the device hostname (e.g., `Home System Local`) instead of a raw IP address. Paradoxically, hostname resolution is faster and more reliable than direct IP for these battery devices — likely due to the short wakeup window and ARP/MQTT broker handshake timing.
- **Button sequence:** First press → OTA mode (blinking).

### Water Leak Sensor (ESP8266 / TYWE3S)

- **Static IP** — same as PIR, static addressing is used in firmware for reliability.
- **OTA procedure:** Identical to PIR — press reset button → blinking → flash OTA within ~1 minute.
- **Battery monitoring via voltage divider:** The water leak sensor outputs a constant voltage from the battery line. An internal voltage divider on the ESP module produces ~1V, which is measured via the ESP's ADC. Battery percentage is calculated from this reading. This is the only reliable method because the TuyaMCU does not transmit battery data over UART — it sleeps between events and never reports battery state.
- **Reset reminder system:** Since the sensor sleeps and does not actively report battery level, a dedicated entity pushes the last session timestamp to Home Assistant. An automation records the date of each communication session and sends a reminder notification (configurable: 6 months, 1 year) to physically reset the leak sensor. This ensures the sensor remains responsive and the battery measurement stays accurate over long periods.
- **Button sequence:** Same as PIR — first press → OTA mode (blinking). Second press → full device reset.

### PIR Motion Sensor (BK7231N / CBU)

- **New device — accidentally acquired.** The general code was adapted for this platform and works, though the UART protocol behaves slightly differently (larger frame structure, but no additional issues observed).
- **MQTT by IP only.** Unlike the ESP8266 variant where a hostname (`Home System Local`) works faster, the BK7231N module requires a **direct IP address** for the MQTT broker. Using a hostname causes significant slowdowns — likely due to DNS resolution overhead in the LibreTiny stack during the short 4–5 second wakeup window.
- **Initial flashing:** Requires `ltchiptool` or the [LibreTiny flasher](https://github.com/libretiny-eu/libretiny).
- **Bootloader serial noise filtering** is especially important on this platform — see [How It Works](#how-it-works).
- **OTA and reset logic:** Same as ESP8266 — press reset button → blinking → flash OTA within ~1 minute.

---

## 📁 Repository Structure

This component follows the **standard ESPHome external component** layout. All three device configurations are included — simply choose the one matching your hardware, fill in your variables (device name, static IP, MQTT broker, Wi-Fi credentials), and flash.

Each device has its own config with pre-filled defaults. The `.cpp` and `.h` component files are shared across all configurations — the code is universal.

---


> **Note:** Replace DP-IDs, pin assignments, and network settings with values matching your specific device. The example above is a starting point — the actual `.cpp` / `.h` component files contain the protocol parser and frame reconstruction logic.

---

## How It Works

### Universal Protocol Initialization

The component uses a **heartbeat-first** initialization sequence:

1. **Heartbeat** — on each wakeup, the component sends a heartbeat frame to the TuyaMCU. If the MCU responds, communication is established immediately.
2. **Product query fallback** — if the heartbeat receives no response within the timeout, the component automatically falls back to a product query (`0x04` command). This requests the device's product key and DP configuration from the MCU.
3. Both paths are fully supported. The heartbeat path is faster and is used on ESP8266 devices. The product query fallback was added during BK7231N development, where the heartbeat was not initially accepted — the MCU required a full product query handshake before responding to any other commands.

### Port Noise Suppression (Unified Setup)

The breakthrough for BK7231N came from a custom `setup()` block that performs **port noise suppression** — cleaning garbage bytes and suppressing noisy GPIO lines before UART communication begins. This solved the BK7231N ↔ TuyaMCU communication failure entirely.

The same algorithm was then applied to the ESP8266 variant — not strictly necessary, but it unified the codebase into a single, reliable initialization sequence. The result is one universal scanner/component that works identically across all three supported devices.

### Problem 1: Bootloader Serial Noise (BK7231N)

On cold boot, the Beken bootloader injects garbage bytes into the UART RX buffer before the TuyaMCU starts sending valid `55 AA` frames. The component filters these by:

1. Scanning the incoming buffer for the `0x55 0xAA` sync header
2. Discarding all bytes before the first valid sync sequence
3. Resetting the parser state machine on each wakeup cycle

### Problem 2: Grouped Frame Buffer Truncation

Battery devices wake for only 4–5 seconds. During this window, the TuyaMCU may send multiple frames back-to-back. The standard ESPHome Tuya component often reads only the first frame and discards the rest. This component:

1. Maintains a **private circular buffer** that accumulates bytes across multiple `loop()` iterations
2. Extracts complete frames by checking the `length` field in the Tuya protocol header
3. Processes all valid frames found in the buffer, not just the first one
4. Clears the buffer on each new wakeup cycle to prevent stale data

---

## Troubleshooting

| Symptom | Cause | Solution |
|--------|-------|----------|
| No data in Home Assistant | TX/RX lines not restored after flashing | Double-check soldering on UART lines |
| Garbage in logs on BK7231N | Bootloader noise not filtered | Ensure you're using the latest component build |
| OTA fails after 60s | Window expired before upload completed | Reduce binary size or use faster OTA speed |
| Device not detected after battery replacement | TuyaMCU needs full power cycle | Remove batteries for 10s, then reinsert |
| Frames appear truncated in logs | Buffer overflow or old component version | Check buffer log lines — buffer should clear on each wake |
| BK7231N extremely slow over MQTT | Hostname used instead of IP | Switch MQTT broker config to direct IP address |
| ESP8266 slow over MQTT | IP address used instead of hostname | Try using hostname (e.g., `Home System Local`) instead |

---

## License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

---

> **⚠️ Disclaimer:** This component is not affiliated with or endorsed by Tuya. Modifying battery-powered safety devices (water leak sensors) carries inherent risk. Always test thoroughly and never rely solely on modified firmware for critical water damage prevention.

