#include "ui_gauge.h"
#include "config.h"

namespace {
    constexpr uint16_t COL_BG          = 0x0842;
    constexpr uint16_t COL_PANEL       = 0x10A4;
    constexpr uint16_t COL_FORD_BLUE   = 0x1D5F;
    constexpr uint16_t COL_STEEL       = 0x4A69;
    constexpr uint16_t COL_TEXT_DIM    = 0x8C71;
    constexpr uint16_t COL_WHITE       = 0xFFFF;
    constexpr uint16_t COL_OK_GREEN    = 0x268B;
    constexpr uint16_t COL_WARN_AMBER  = 0xFD60;
    constexpr uint16_t COL_CRIT_RED    = 0xF986;
}

void RoundGaugeUI::begin() {
    _tft.init();
    _tft.setRotation(0);
    _tft.setBrightness(220);
    _tft.fillScreen(TFT_BLACK);

    _canvas.setColorDepth(16);
    _canvas.createSprite(240, 240);
}

void RoundGaugeUI::nextPage() {
    uint8_t next = (static_cast<uint8_t>(_page) + 1) % static_cast<uint8_t>(GaugePage::PAGE_COUNT);
    _page = static_cast<GaugePage>(next);
}

void RoundGaugeUI::render(const VgateObdBle& obd) {
    const FlexVehicleData& data = obd.getData();
    _canvas.fillSprite(COL_BG);

    drawOuterBezel(data);

    switch (_page) {
        case GaugePage::DEBUG_LOG:
            renderDebugLogPage(obd);
            break;
        case GaugePage::ALL_IN_ONE:
            renderAllInOnePage(data);
            break;
        case GaugePage::TPMS_LARGE:
            renderTpmsLargePage(data);
            break;
        case GaugePage::TEMPS_LARGE:
            renderTempsLargePage(data);
            break;
        default:
            renderAllInOnePage(data);
            break;
    }

    _canvas.pushSprite(&_tft, 0, 0);
}

void RoundGaugeUI::renderDebugLogPage(const VgateObdBle& obd) {
    const FlexVehicleData& data = obd.getData();

    _canvas.setFont(&fonts::Font2);
    _canvas.setTextDatum(top_center);
    _canvas.setTextColor(COL_FORD_BLUE);
    _canvas.drawString("BLE / ELM DEBUG LOG", 120, 14);
    _canvas.drawFastHLine(34, 32, 172, COL_STEEL);

    String lines[VgateObdBle::MAX_LOG_LINES];
    int count = 0;
    obd.getLogLines(lines, count);

    _canvas.setFont(&fonts::Font0);
    _canvas.setTextDatum(top_left);

    int startY = 38;
    int lineH  = 14;
    for (int i = 0; i < count; i++) {
        String& ln = lines[i];
        uint16_t col = COL_WHITE;
        if (ln.startsWith("ERR") || ln.indexOf("Fail") >= 0 || ln.indexOf("No reply") >= 0) {
            col = COL_CRIT_RED;
        } else if (ln.startsWith("RX:") || ln.startsWith("LOCKED") || ln.indexOf("Ready") >= 0 || ln.startsWith("LF:")) {
            col = COL_OK_GREEN;
        } else if (ln.startsWith("Probe") || ln.startsWith("TX") || ln.startsWith("Found")) {
            col = COL_WARN_AMBER;
        } else if (ln.startsWith("Sub") || ln.startsWith("Svcs") || ln.startsWith("MAC")) {
            col = COL_FORD_BLUE;
        }
        _canvas.setTextColor(col);
        _canvas.drawString(ln, 38, startY + i * lineH);
    }

    drawStatusPill(data, 206);
}

void RoundGaugeUI::drawOuterBezel(const FlexVehicleData& data) {
    uint16_t ringColor = COL_FORD_BLUE;
    if (data.state == ObdState::DEMO_MODE) {
        ringColor = COL_WARN_AMBER;
    } else if (data.state != ObdState::READY) {
        ringColor = COL_STEEL;
    }

    _canvas.drawCircle(120, 120, 119, ringColor);
    _canvas.drawCircle(120, 120, 118, ringColor);
    _canvas.drawCircle(120, 120, 114, COL_PANEL);
}

void RoundGaugeUI::drawStatusPill(const FlexVehicleData& data, int y) {
    uint16_t dotCol = COL_WARN_AMBER;
    if (data.state == ObdState::READY) dotCol = COL_OK_GREEN;
    else if (data.state == ObdState::DEMO_MODE) dotCol = COL_FORD_BLUE;

    _canvas.fillRoundRect(56, y, 128, 16, 8, COL_PANEL);
    _canvas.fillCircle(66, y + 8, 3, dotCol);

    _canvas.setTextDatum(middle_center);
    _canvas.setFont(&fonts::Font0);
    _canvas.setTextColor(COL_TEXT_DIM);
    _canvas.drawString(data.statusText, 124, y + 8);
}

void RoundGaugeUI::drawFlexCarIcon(int cx, int cy, const FlexVehicleData& data) {
    const int carW = 36;
    const int carH = 74;
    const int left = cx - carW / 2;
    const int top  = cy - carH / 2;

    uint16_t cLF = getTireColor(data.tireLF);
    uint16_t cRF = getTireColor(data.tireRF);
    uint16_t cLR = getTireColor(data.tireLR);
    uint16_t cRR = getTireColor(data.tireRR);

    _canvas.fillRoundRect(left - 5, top + 9,  5, 15, 2, cLF);
    _canvas.fillRoundRect(left + carW, top + 9, 5, 15, 2, cRF);
    _canvas.fillRoundRect(left - 5, top + carH - 23, 5, 15, 2, cLR);
    _canvas.fillRoundRect(left + carW, top + carH - 23, 5, 15, 2, cRR);

    _canvas.fillRoundRect(left, top, carW, carH, 7, COL_PANEL);
    _canvas.drawRoundRect(left, top, carW, carH, 7, COL_FORD_BLUE);

    _canvas.fillRoundRect(left + 4, top + 13, carW - 8, 11, 3, COL_STEEL);
    _canvas.fillRoundRect(left + 5, top + carH - 10, carW - 10, 6, 2, COL_STEEL);

    for (int gx = left + 8; gx <= left + carW - 8; gx += 5) {
        _canvas.drawFastVLine(gx, top + 27, 34, COL_STEEL);
    }
}

void RoundGaugeUI::renderAllInOnePage(const FlexVehicleData& data) {
    char buf[24];

    _canvas.drawFastHLine(32, 54, 176, COL_PANEL);
    _canvas.drawFastVLine(120, 12, 42, COL_PANEL);

    _canvas.setFont(&fonts::Font0);
    _canvas.setTextDatum(top_center);
    _canvas.setTextColor(COL_TEXT_DIM);
    _canvas.drawString("COOLANT", 80, 15);
    _canvas.drawString("TRANS", 160, 15);

    _canvas.setFont(&fonts::Font4);
    _canvas.setTextColor(getCoolantColor(data.coolantTempF));
    if (data.coolantTempF > -100.0f) snprintf(buf, sizeof(buf), "%.0fF", data.coolantTempF);
    else snprintf(buf, sizeof(buf), "--F");
    _canvas.drawString(buf, 78, 27);

    _canvas.setTextColor(getTransColor(data.transTempF));
    if (data.transTempF > -100.0f) snprintf(buf, sizeof(buf), "%.0fF", data.transTempF);
    else snprintf(buf, sizeof(buf), "--F");
    _canvas.drawString(buf, 162, 27);

    drawFlexCarIcon(120, 117, data);

    auto drawCornerTire = [&](int x, int y, const char* label, float psi, bool alignRight) {
        _canvas.setFont(&fonts::Font0);
        _canvas.setTextColor(COL_TEXT_DIM);
        _canvas.setTextDatum(alignRight ? top_right : top_left);
        _canvas.drawString(label, x, y);

        _canvas.setFont(&fonts::Font4);
        _canvas.setTextColor(getTireColor(psi));
        if (psi >= 0.0f) snprintf(buf, sizeof(buf), "%.1f", psi);
        else snprintf(buf, sizeof(buf), "--.-");
        _canvas.drawString(buf, x, y + 10);
    };

    drawCornerTire(24,  72,  "LF PSI", data.tireLF, false);
    drawCornerTire(216, 72,  "RF PSI", data.tireRF, true);
    drawCornerTire(24,  122, "LR PSI", data.tireLR, false);
    drawCornerTire(216, 122, "RR PSI", data.tireRR, true);

    _canvas.drawFastHLine(32, 174, 176, COL_PANEL);
    _canvas.drawFastVLine(120, 174, 34, COL_PANEL);

    _canvas.setFont(&fonts::Font0);
    _canvas.setTextDatum(top_center);
    _canvas.setTextColor(COL_TEXT_DIM);
    _canvas.drawString("INTAKE", 80, 178);
    _canvas.drawString("BATTERY", 160, 178);

    _canvas.setFont(&fonts::Font2);
    _canvas.setTextColor(COL_WHITE);
    if (data.intakeTempF > -100.0f) snprintf(buf, sizeof(buf), "%.0f F", data.intakeTempF);
    else snprintf(buf, sizeof(buf), "-- F");
    _canvas.drawString(buf, 80, 189);

    _canvas.setTextColor(getVoltColor(data.batteryVolt));
    if (data.batteryVolt > 0.0f) snprintf(buf, sizeof(buf), "%.1f V", data.batteryVolt);
    else snprintf(buf, sizeof(buf), "--.- V");
    _canvas.drawString(buf, 160, 189);

    drawStatusPill(data, 211);
}

void RoundGaugeUI::renderTpmsLargePage(const FlexVehicleData& data) {
    char buf[24];

    _canvas.setFont(&fonts::Font2);
    _canvas.setTextDatum(top_center);
    _canvas.setTextColor(COL_FORD_BLUE);
    _canvas.drawString("TIRE PRESSURE (PSI)", 120, 20);

    _canvas.drawFastHLine(24, 122, 192, COL_PANEL);
    _canvas.drawFastVLine(120, 46, 152, COL_PANEL);

    auto drawBigQuad = [&](int cx, int cy, const char* pos, float psi) {
        _canvas.setFont(&fonts::Font2);
        _canvas.setTextDatum(top_center);
        _canvas.setTextColor(COL_TEXT_DIM);
        _canvas.drawString(pos, cx, cy - 28);

        _canvas.setFont(&fonts::Font6);
        _canvas.setTextColor(getTireColor(psi));
        if (psi >= 0.0f) snprintf(buf, sizeof(buf), "%.1f", psi);
        else snprintf(buf, sizeof(buf), "--");
        _canvas.drawString(buf, cx, cy - 10);
    };

    drawBigQuad(74,  84,  "FL", data.tireLF);
    drawBigQuad(166, 84,  "FR", data.tireRF);
    drawBigQuad(74,  160, "RL", data.tireLR);
    drawBigQuad(166, 160, "RR", data.tireRR);

    drawStatusPill(data, 206);
}

void RoundGaugeUI::renderTempsLargePage(const FlexVehicleData& data) {
    char buf[24];

    _canvas.setFont(&fonts::Font2);
    _canvas.setTextDatum(top_center);
    _canvas.setTextColor(COL_FORD_BLUE);
    _canvas.drawString("3.5L / 6F50 VITALS", 120, 20);

    auto drawRow = [&](int y, const char* label, const char* valStr, uint16_t col) {
        _canvas.fillRoundRect(34, y, 172, 34, 6, COL_PANEL);
        _canvas.setFont(&fonts::Font2);
        _canvas.setTextDatum(middle_left);
        _canvas.setTextColor(COL_TEXT_DIM);
        _canvas.drawString(label, 44, y + 17);

        _canvas.setFont(&fonts::Font4);
        _canvas.setTextDatum(middle_right);
        _canvas.setTextColor(col);
        _canvas.drawString(valStr, 196, y + 17);
    };

    if (data.transTempF > -100.0f) snprintf(buf, sizeof(buf), "%.0f F", data.transTempF);
    else snprintf(buf, sizeof(buf), "--- F");
    drawRow(46, "TRANS", buf, getTransColor(data.transTempF));

    if (data.coolantTempF > -100.0f) snprintf(buf, sizeof(buf), "%.0f F", data.coolantTempF);
    else snprintf(buf, sizeof(buf), "--- F");
    drawRow(85, "COOLANT", buf, getCoolantColor(data.coolantTempF));

    if (data.intakeTempF > -100.0f) snprintf(buf, sizeof(buf), "%.0f F", data.intakeTempF);
    else snprintf(buf, sizeof(buf), "--- F");
    drawRow(124, "INTAKE", buf, COL_WHITE);

    if (data.batteryVolt > 0.0f) snprintf(buf, sizeof(buf), "%.2f V", data.batteryVolt);
    else snprintf(buf, sizeof(buf), "--.- V");
    drawRow(163, "BATTERY", buf, getVoltColor(data.batteryVolt));

    drawStatusPill(data, 206);
}

uint16_t RoundGaugeUI::getTireColor(float psi) {
    if (psi < 0.0f) return COL_TEXT_DIM;
    if (psi <= TIRE_PSI_LOW_CRIT || psi >= TIRE_PSI_HIGH_CRIT) return COL_CRIT_RED;
    if (psi <= TIRE_PSI_LOW_WARN || psi >= TIRE_PSI_HIGH_WARN) return COL_WARN_AMBER;
    return COL_OK_GREEN;
}

uint16_t RoundGaugeUI::getTransColor(float tempF) {
    if (tempF < -100.0f) return COL_TEXT_DIM;
    if (tempF >= TRANS_TEMP_CRIT_F) return COL_CRIT_RED;
    if (tempF >= TRANS_TEMP_WARN_F) return COL_WARN_AMBER;
    return COL_OK_GREEN;
}

uint16_t RoundGaugeUI::getCoolantColor(float tempF) {
    if (tempF < -100.0f) return COL_TEXT_DIM;
    if (tempF >= COOLANT_TEMP_CRIT_F) return COL_CRIT_RED;
    if (tempF >= COOLANT_TEMP_WARN_F) return COL_WARN_AMBER;
    return COL_OK_GREEN;
}

uint16_t RoundGaugeUI::getVoltColor(float volt) {
    if (volt <= 0.0f) return COL_TEXT_DIM;
    if (volt <= BATT_VOLT_LOW_CRIT) return COL_CRIT_RED;
    if (volt <= BATT_VOLT_LOW_WARN) return COL_WARN_AMBER;
    return COL_OK_GREEN;
}
