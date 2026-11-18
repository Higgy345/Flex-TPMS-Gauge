#include <Arduino.h>
#include "config.h"
#include "obd_ble.h"
#include "ui_gauge.h"

VgateObdBle obd;
RoundGaugeUI gauge;

// Background FreeRTOS task for BLE scanning & ELM327 polling
// Keeps the round TFT UI and BOOT button 100% responsive even during OBD waits
void obdTask(void* pvParameters) {
    for (;;) {
        obd.update();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void handleBootButton() {
    static bool lastBtnState = HIGH;
    static uint32_t pressStartMs = 0;
    static bool longPressHandled = false;

    bool currentState = digitalRead(PIN_BOOT_BTN);
    uint32_t now = millis();

    if (lastBtnState == HIGH && currentState == LOW) {
        // Button just pressed
        pressStartMs = now;
        longPressHandled = false;
    } else if (lastBtnState == LOW && currentState == LOW) {
        // Button held down >= 1200ms -> Toggle Demo / Live Mode
        if (!longPressHandled && (now - pressStartMs >= 1200)) {
            longPressHandled = true;
            obd.toggleDemoMode();
        }
    } else if (lastBtnState == LOW && currentState == HIGH) {
        // Button released -> Short press switches gauge screen
        if (!longPressHandled && (now - pressStartMs >= 40)) {
            gauge.nextPage();
        }
    }

    lastBtnState = currentState;
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
    pinMode(PIN_STATUS_LED, OUTPUT);
    digitalWrite(PIN_STATUS_LED, HIGH); // Active-LOW LED off initially

    // 1. Initialize 1.28" 240x240 Round GC9A01 Display
    gauge.begin();

    // 2. Initialize Vgate iCar Pro BLE 4.0 Client
    obd.begin(START_IN_DEMO_MODE);

    // 3. Start non-blocking OBD polling task
    xTaskCreate(
        obdTask,
        "OBD_BLE_Task",
        8192,
        nullptr,
        1,
        nullptr
    );

    Serial.println("2018 Ford Flex Round Gauge Initialized!");
    Serial.println("- Short-press BOOT (GPIO 9): Cycle Gauge Screens");
    Serial.println("- Long-press BOOT (1.2s): Toggle Bench Demo Mode vs Live BLE");
}

void loop() {
    handleBootButton();

    // Render UI at ~15 FPS
    static uint32_t lastRenderMs = 0;
    uint32_t now = millis();
    if (now - lastRenderMs >= 66) {
        lastRenderMs = now;
        const FlexVehicleData& data = obd.getData();
        gauge.render(obd);

        // Status LED: solid ON when connected to car, slow blink when scanning
        if (data.state == ObdState::READY) {
            digitalWrite(PIN_STATUS_LED, LOW); // Active LOW = ON
        } else {
            digitalWrite(PIN_STATUS_LED, (now / 500) % 2);
        }
    }

    delay(10);
}
