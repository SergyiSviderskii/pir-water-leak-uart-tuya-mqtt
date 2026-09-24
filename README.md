# pir-water-leak-uart-tuya-mqtt
Universal ESPHome C++ component for TuyaMCU Version 0 battery sensors. Supports ESP8266 &amp; BK7231N (LibreTiny). Fixes Beken bootloader serial noise and grouped frame buffer truncation.
## 🔧 Environment & Build Target

This component has been verified and compiled using the production release of ESPHome:
* **ESPHome Version:** `2026.9.0`
* **Configuration Hash:** `0xa44d4e42`
* **Build Timestamp:** `2026-09-21 21:42:27 +0300`

---

## ⚡ First-Time Flashing (Flashing "By Wire")

Since battery-powered devices keep the Wi-Fi module completely powered down until a physical sensor event or button press occurs, performing the initial firmware installation requires temporary hardware modifications:

1. **Isolate the MCU:** Disconnect or desolder the `TX` and `RX` lines between the TuyaMCU and the Wi-Fi module (`TYWE3S` or `CBU`) to prevent serial bus contention during programming.
2. **Apply Stable External Power:** Provide a solid, continuous `3.3V` power supply directly to the Wi-Fi chip from your USB-to-UART adapter (ensure common ground `GND`). Do not rely on the device's battery lines during this stage.
3. **Flash the Binary:** Use your standard serial flashing utility (e.g., `esptool.py` for ESP8266 or `ltchiptool` / LibreTiny flasher for BK7231N) to burn the initial ESPHome base image over the wired connection.
4. **Restore the Bus:** Once successfully flashed, reconnect the UART lines back to the TuyaMCU.

---

## 🔘 Device Reset & OTA Firmware Update Logic

Once the device is assembled and running on battery power, it is impossible to flash it over-the-air (OTA) under normal operation because the chip cuts its own power line within 4–5 seconds. 

To bypass this and push updates wirelessly, use the **Physical Reset/Pairing Button**:

1. **Trigger the Timeout Loop:** Press and hold the physical button inside the device until the status LEDs begin to blink rapidly. 
2. **MCU Hold-On State:** The TuyaMCU detects this hardware override sequence and enters an autoconfiguration/pairing mode. Instead of shutting down after 4 seconds, the MCU will force-keep the Wi-Fi chip powered on for approximately **60-180 seconds**, waiting for a network handshake.
3. **Executing OTA:** Within this 35-second window, your automation scripts must initiate the wireless ESPHome OTA update process. 
4. **Automatic Reboot:** If the OTA fails or times out, the device will automatically reboot after ~60 seconds, cut the power rail, and return to its ultra-low-power event-driven sleep routine.

[22:56:42][E][main:215]:   [Sending] -----> [55 AA 00 02 00 01 04 06 ] Время: 269 мс
[22:56:42][E][main:215]:   [Received] <----- [55 AA 00 02 00 00 01 ] Время: 286 мс
[22:56:42][E][main:224]: === Приватный буфер успешно очищен ===
[22:56:42][E][main:230]: ===============================
[22:56:42][E][tuya:220]: =========WIFI OTA=========
[22:56:42][E][tuya:221]: [Received] <-----:FRAME=[55.AA.00.04.00.01.00.04 (8)] 4874  
[22:56:42][E][tuya:364]:    [Sending] -----> [55.AA.00.05.00.01.00.05 (8)] Время: 4875 мс
