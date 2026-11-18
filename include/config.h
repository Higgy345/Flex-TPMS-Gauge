#pragma once
#include <Arduino.h>

// ============================================================================
// 1. ESP32-C3 SUPER MINI PIN DEFINITIONS (1.28" GC9A01 240x240 Round TFT)
// ============================================================================
#define PIN_TFT_SCLK  4   // SCL / SCK / CLK
#define PIN_TFT_MOSI  6   // SDA / MOSI / DIN
#define PIN_TFT_CS    7   // CS
#define PIN_TFT_DC    2   // DC
#define PIN_TFT_RST   3   // RST
#define PIN_TFT_BL    1   // BLK (Backlight PWM - optional, or tie BLK to 3.3V)

#define PIN_BOOT_BTN  9   // Built-in BOOT button on ESP32-C3 Super Mini
#define PIN_STATUS_LED 8  // Built-in blue LED on ESP32-C3 Super Mini (Active LOW)

// ============================================================================
// 2. VGATE iCAR PRO BLE 4.0 CONFIGURATION
// ============================================================================
// The Vgate iCar Pro BLE 4.0 advertises as "IOS-Vlink" or "Vgate" with Service 0x18F0
#define VGATE_SERVICE_UUID        "000018f0-0000-1000-8000-00805f9b34fb"
#define VGATE_CHAR_NOTIFY_UUID    "00002af0-0000-1000-8000-00805f9b34fb" // Adapter -> ESP32
#define VGATE_CHAR_WRITE_UUID     "00002af1-0000-1000-8000-00805f9b34fb" // ESP32 -> Adapter

// Timeouts & polling intervals (milliseconds)
#define OBD_CMD_TIMEOUT_MS        1200  // Max wait for a single ELM327 command
#define TPMS_POLL_INTERVAL_MS     5000  // Poll 4 tires every 5 seconds
#define ENGINE_POLL_INTERVAL_MS   1000  // Poll engine/trans temps & voltage every 1 second

// ============================================================================
// 3. 2018 FORD FLEX LIMITED (3.5L NA / 6F50) OBD2 & ENHANCED PIDs
// ============================================================================
// CAN Module Headers:
#define HEADER_BCM  "726"   // Body Control Module (handles TPMS on 2018 Flex)
#define HEADER_PCM  "7E0"   // Powertrain Control Module (Engine & 6F50 Transmission)

// BCM (726) Mode 22 PIDs - Individual Tire Pressures:
// Formula (PSI): (((256 * A) + B) / 3.0 + (22.0 / 3.0)) * 0.145
#define PID_TPMS_LF  "222813"  // Left Front (Driver Front)
#define PID_TPMS_RF  "222814"  // Right Front (Passenger Front)
#define PID_TPMS_RR  "222815"  // Right Rear (Passenger Rear)
#define PID_TPMS_LR  "222816"  // Left Rear (Driver Rear)

// PCM (7E0) Mode 22 & Mode 01 PIDs:
// 6F50 Transmission Fluid Temp (TFT) - Formula (°F): (((signed(A)*256)+B)*(9.0/80.0)+32.0)
#define PID_TRANS_TEMP   "221E1C"
// Standard Mode 01 Coolant Temp (ECT) - Formula (°F): ((A - 40) * 1.8) + 32
#define PID_COOLANT_TEMP "0105"
// Standard Mode 01 Intake Air Temp (IAT) - Formula (°F): ((A - 40) * 1.8) + 32
#define PID_INTAKE_TEMP  "010F"
// Adapter Voltage Command
#define CMD_BATTERY_VOLT "ATRV"

// ============================================================================
// 4. GAUGE WARNING THRESHOLDS (2018 Ford Flex Recommended Door-Jamb PSI: 35 PSI)
// ============================================================================
#define TIRE_PSI_TARGET     35.0f
#define TIRE_PSI_LOW_WARN   30.0f  // Yellow below 30 PSI
#define TIRE_PSI_LOW_CRIT   26.0f  // Red below 26 PSI
#define TIRE_PSI_HIGH_WARN  41.0f  // Yellow above 41 PSI
#define TIRE_PSI_HIGH_CRIT  45.0f  // Red above 45 PSI

#define TRANS_TEMP_WARN_F   215.0f // Yellow above 215°F
#define TRANS_TEMP_CRIT_F   235.0f // Red above 235°F

#define COOLANT_TEMP_WARN_F 218.0f // Yellow above 218°F
#define COOLANT_TEMP_CRIT_F 230.0f // Red above 230°F

#define BATT_VOLT_LOW_WARN  12.2f  // Yellow below 12.2V (engine off) /charging fault
#define BATT_VOLT_LOW_CRIT  11.7f  // Red below 11.7V

// Set to true if you want the gauge to boot straight into Demo Mode on your desk
// (You can also hold the BOOT button for 1.5 seconds at any time to toggle Demo Mode)
#define START_IN_DEMO_MODE  false
