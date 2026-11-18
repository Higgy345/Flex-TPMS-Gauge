<img width="271" height="365" alt="Screenshot 2026-11-18 164741" src="https://github.com/user-attachments/assets/d9b48de3-aaf1-436d-9070-3f7e67b3e2a9" />
# 2018 Ford Flex (3.5L NA / 6F50) Round OBD2 BLE Gauge

Custom auxiliary gauge built on an **ESP32-C3 Super Mini** and a **1.28" 240x240 Round IPS TFT (GC9A01)** communicating wirelessly with a **Vgate iCar Pro BLE 4.0** OBD2 adapter.

---

## 1. Wiring Diagram (ESP32-C3 Super Mini to 1.28" GC9A01 Round TFT)

| 1.28" Round TFT Pin | ESP32-C3 Super Mini Pin | Function |
| :--- | :--- | :--- |
| **VCC** | `3V3` | 3.3V Power |
| **GND** | `GND` | Ground |
| **SCL / SCK / CLK** | `GPIO 4` | Hardware SPI Clock |
| **SDA / MOSI / DIN** | `GPIO 6` | Hardware SPI MOSI |
| **CS** | `GPIO 7` | Chip Select |
| **DC** | `GPIO 2` | Data / Command |
| **RST** | `GPIO 3` | Reset |
| **BLK** *(if present)* | `GPIO 1` (or `3V3`) | Backlight PWM |

---

## 2. Gauge Views & Controls (Onboard `BOOT` Button - GPIO 9)

* **Short Press (`BOOT` button):** Cycles between 3 screens:
  1. **All-in-One Dashboard:** Top-down Ford Flex silhouette with 4 color-coded individual tire pressures (`LF`, `RF`, `LR`, `RR`), plus **Coolant Temp** & **Trans Temp** at the top, and **Intake Air Temp** & **Battery Voltage** at the bottom.
  2. **Big 4-Corner TPMS Screen:** Extra-large font for the 4 tire pressures (`FL`, `FR`, `RL`, `RR`).
  3. **3.5L / 6F50 Vitals Screen:** Large readouts for Transmission Fluid Temp, Engine Coolant Temp, Intake Air Temp, and Battery Voltage.
* **Long Press (`BOOT` button for 1.2 sec):** Toggles **Bench Demo Mode** on/off so you can test the screens and color alerts on your desk before plugging into the car.

---

## 3. 2018 Ford Flex PIDs Used

| Parameter | Module Header | Mode + PID | Formula |
| :--- | :--- | :--- | :--- |
| **Left Front Tire (PSI)** | `726` (BCM) | `222813` | `(((256 * A) + B) / 3 + 22/3) * 0.145038` |
| **Right Front Tire (PSI)** | `726` (BCM) | `222814` | `(((256 * A) + B) / 3 + 22/3) * 0.145038` |
| **Right Rear Tire (PSI)** | `726` (BCM) | `222815` | `(((256 * A) + B) / 3 + 22/3) * 0.145038` |
| **Left Rear Tire (PSI)** | `726` (BCM) | `222816` | `(((256 * A) + B) / 3 + 22/3) * 0.145038` |
| **6F50 Trans Fluid Temp (°F)** | `7E0` (PCM) | `221E1C` | `((signed(A*256 + B)) * (9 / 80)) + 32` |
| **Engine Coolant Temp (°F)** | `7E0` (PCM) | `0105` | `((A - 40) * 1.8) + 32` |
| **Intake Air Temp (°F)** | `7E0` (PCM) | `010F` | `((A - 40) * 1.8) + 32` |
| **Battery Voltage (V)** | Adapter | `ATRV` | Direct float parse |
