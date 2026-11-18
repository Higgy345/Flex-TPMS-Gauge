#pragma once
#include <Arduino.h>
#include "display_config.h"
#include "obd_ble.h"

enum class GaugePage : uint8_t {
    ALL_IN_ONE  = 0,  // Default boot screen: Car silhouette + 4 tires + temps/volts
    TPMS_LARGE  = 1,
    TEMPS_LARGE = 2,
    DEBUG_LOG   = 3,  // Verbose troubleshooting log (still accessible via BOOT button)
    PAGE_COUNT  = 4
};

class RoundGaugeUI {
public:
    void begin();
    void render(const VgateObdBle& obd);
    void nextPage();
    GaugePage getPage() const { return _page; }

private:
    LGFX_RoundTFT _tft;
    LGFX_Sprite   _canvas;
    GaugePage     _page = GaugePage::ALL_IN_ONE;

    void drawOuterBezel(const FlexVehicleData& data);
    void drawStatusPill(const FlexVehicleData& data, int y);
    void drawFlexCarIcon(int cx, int cy, const FlexVehicleData& data);

    void renderDebugLogPage(const VgateObdBle& obd);
    void renderAllInOnePage(const FlexVehicleData& data);
    void renderTpmsLargePage(const FlexVehicleData& data);
    void renderTempsLargePage(const FlexVehicleData& data);

    uint16_t getTireColor(float psi);
    uint16_t getTransColor(float tempF);
    uint16_t getCoolantColor(float tempF);
    uint16_t getVoltColor(float volt);
};
