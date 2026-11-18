#include "obd_ble.h"
#include "config.h"
#include <NimBLEDevice.h>
#include <vector>

namespace {
    constexpr size_t MAX_RX_BUFFER_LEN = 256;

    NimBLEAddress           g_targetAddress;
    String                  g_targetName = "";
    NimBLEClient*           g_pClient      = nullptr;

    // Active locked write characteristic & mode once probed
    NimBLERemoteCharacteristic* g_pCharWrite = nullptr;
    bool                        g_writeWithResp = false;

    // Candidate write characteristics discovered on the Vgate adapter
    std::vector<NimBLERemoteCharacteristic*> g_writeCandidates;

    volatile bool g_doConnect = false;
    volatile bool g_connected = false;
    volatile bool g_promptReceived = false;
    String g_rxBuffer = "";

    // Spinlocks for cross-task thread safety (NimBLE task <-> OBD task <-> UI loop)
    portMUX_TYPE g_rxMux   = portMUX_INITIALIZER_UNLOCKED;
    portMUX_TYPE g_dataMux = portMUX_INITIALIZER_UNLOCKED;
    portMUX_TYPE g_logMux  = portMUX_INITIALIZER_UNLOCKED;

    // Ring buffer of debug lines for on-screen verbose terminal
    String g_logLines[VgateObdBle::MAX_LOG_LINES];
    int    g_logCount = 0;

    String shortUuid(const NimBLEUUID& uuid) {
        String s = String(uuid.toString().c_str());
        if (s.startsWith("0000") && s.length() >= 8) {
            return s.substring(4, 8);
        }
        if (s.length() > 8) {
            return s.substring(0, 8);
        }
        return s;
    }

    // Notification callback (Vgate -> ESP32) with bounded buffer & spinlock
    void notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
        portENTER_CRITICAL(&g_rxMux);
        for (size_t i = 0; i < length; i++) {
            char c = static_cast<char>(pData[i]);
            if (c == '>') {
                g_promptReceived = true;
            } else if (c != '\0' && g_rxBuffer.length() < MAX_RX_BUFFER_LEN) {
                g_rxBuffer += c;
            }
        }
        portEXIT_CRITICAL(&g_rxMux);
    }

    class ClientCallbacks : public NimBLEClientCallbacks {
        void onConnect(NimBLEClient* pClient) override {
            g_connected = true;
            VgateObdBle::addLog("BLE Connected!");
        }

        void onDisconnect(NimBLEClient* pClient) override {
            g_connected = false;
            VgateObdBle::addLog("BLE Disconnected");
        }

        uint32_t onPassKeyRequest() override {
            VgateObdBle::addLog("PassKey req -> 1234");
            return 1234;
        }

        bool onConfirmPIN(uint32_t pin) override {
            VgateObdBle::addLog("Confirm PIN: " + String(pin));
            return true;
        }

        void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
            if (desc->sec_state.encrypted) {
                VgateObdBle::addLog("Auth: Encrypted OK");
            } else {
                VgateObdBle::addLog("Auth: Unencrypted");
            }
        }
    };

    class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
        void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
            std::string devName = advertisedDevice->getName();
            String nameStr = String(devName.c_str());
            String nameUpper = nameStr;
            nameUpper.toUpperCase();

            bool matchService =
                advertisedDevice->isAdvertisingService(NimBLEUUID("18f0")) ||
                advertisedDevice->isAdvertisingService(NimBLEUUID("ffe0")) ||
                advertisedDevice->isAdvertisingService(NimBLEUUID("fff0")) ||
                advertisedDevice->isAdvertisingService(NimBLEUUID(VGATE_SERVICE_UUID));

            bool matchName =
                (nameUpper.indexOf("VLINK") >= 0) ||
                (nameUpper.indexOf("V-LINK") >= 0) ||
                (nameUpper.indexOf("VGATE") >= 0) ||
                (nameUpper.indexOf("ICAR") >= 0) ||
                (nameUpper.indexOf("OBD") >= 0) ||
                (nameUpper.indexOf("ELM") >= 0);

            if (matchService || matchName) {
                g_targetAddress = advertisedDevice->getAddress();
                g_targetName    = nameStr.length() > 0 ? nameStr : "Vgate-BLE";

                VgateObdBle::addLog("Found: " + g_targetName);
                VgateObdBle::addLog("MAC: " + String(g_targetAddress.toString().c_str()));

                g_doConnect = true;
                NimBLEDevice::getScan()->stop();
            }
        }
    };

    ClientCallbacks g_clientCallbacks;
    AdvertisedDeviceCallbacks g_scanCallbacks;
}

void VgateObdBle::setStatus(ObdState state, const char* text) {
    portENTER_CRITICAL(&g_dataMux);
    _data.state = state;
    strncpy(_data.statusText, text, sizeof(_data.statusText) - 1);
    _data.statusText[sizeof(_data.statusText) - 1] = '\0';
    portEXIT_CRITICAL(&g_dataMux);
}

FlexVehicleData VgateObdBle::getData() const {
    portENTER_CRITICAL(&g_dataMux);
    FlexVehicleData copy = _data;
    portEXIT_CRITICAL(&g_dataMux);
    return copy;
}

void VgateObdBle::addLog(const String& line) {
    Serial.println("[LOG] " + line);
    String clipped = line.length() > 26 ? line.substring(0, 26) : line;
    portENTER_CRITICAL(&g_logMux);
    if (g_logCount < MAX_LOG_LINES) {
        g_logLines[g_logCount++] = clipped;
    } else {
        for (int i = 0; i < MAX_LOG_LINES - 1; i++) {
            g_logLines[i] = g_logLines[i + 1];
        }
        g_logLines[MAX_LOG_LINES - 1] = clipped;
    }
    portEXIT_CRITICAL(&g_logMux);
}

void VgateObdBle::getLogLines(String outLines[MAX_LOG_LINES], int& outCount) const {
    portENTER_CRITICAL(&g_logMux);
    outCount = g_logCount;
    for (int i = 0; i < g_logCount; i++) {
        outLines[i] = g_logLines[i];
    }
    portEXIT_CRITICAL(&g_logMux);
}

void VgateObdBle::begin(bool startInDemo) {
    _demoMode = startInDemo;

    addLog("Init NimBLE Client...");
    NimBLEDevice::init("Flex-Gauge-C3");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityPasskey(1234);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_KEYBOARD_ONLY);

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(&g_scanCallbacks, true);
    pScan->setInterval(100);
    pScan->setWindow(99);
    pScan->setActiveScan(true);

    if (_demoMode) {
        setStatus(ObdState::DEMO_MODE, "DEMO MODE");
        addLog("Started in DEMO MODE");
        updateDemoValues();
    } else {
        setStatus(ObdState::SCANNING, "Scan: IOS-Vlink...");
        addLog("Scanning for Vgate BLE...");
        pScan->start(5, false);
    }
}

void VgateObdBle::toggleDemoMode() {
    _demoMode = !_demoMode;
    if (_demoMode) {
        if (NimBLEDevice::getScan()->isScanning()) {
            NimBLEDevice::getScan()->stop();
        }
        setStatus(ObdState::DEMO_MODE, "DEMO MODE");
        addLog("Switched -> DEMO MODE");
        updateDemoValues();
    } else {
        portENTER_CRITICAL(&g_dataMux);
        _data = FlexVehicleData();
        portEXIT_CRITICAL(&g_dataMux);

        if (g_connected) {
            setStatus(ObdState::READY, "Live OBD2");
        } else {
            setStatus(ObdState::SCANNING, "Scan: IOS-Vlink...");
            addLog("Switched -> LIVE BLE");
            NimBLEDevice::getScan()->start(5, false);
        }
    }
}

void VgateObdBle::update() {
    uint32_t now = millis();

    if (_demoMode) {
        static uint32_t lastDemoTick = 0;
        if (now - lastDemoTick >= 500) {
            lastDemoTick = now;
            updateDemoValues();
        }
        return;
    }

    if (g_doConnect) {
        g_doConnect = false;
        setStatus(ObdState::CONNECTING, "Connecting BLE...");

        if (connectToServer()) {
            setStatus(ObdState::INITIALIZING_ELM, "Probing ELM...");
            if (probeAndInitElm327()) {
                setStatus(ObdState::READY, "Live OBD2");
                addLog("ELM327 Ready! Polling...");
                pollEngineAndVolts();
                pollTpms();
            } else {
                setStatus(ObdState::INITIALIZING_ELM, "ELM Probe Fail");
                addLog("ELM Probe failed! Wait 4s");
                delay(4000);
                if (g_pClient) g_pClient->disconnect();
            }
        } else {
            setStatus(ObdState::SCANNING, "Conn Retry...");
            delay(2000);
        }
    }

    if (!g_connected) {
        static uint32_t lastScanCheck = 0;
        if (now - lastScanCheck >= 4000) {
            lastScanCheck = now;
            if (!NimBLEDevice::getScan()->isScanning()) {
                setStatus(ObdState::SCANNING, "Scan: IOS-Vlink...");
                addLog("Scanning BLE...");
                NimBLEDevice::getScan()->clearResults();
                NimBLEDevice::getScan()->start(4, false);
            }
        }
        return;
    }

    static uint32_t lastEnginePoll = 0;
    static uint32_t lastTpmsPoll   = 0;

    if (now - lastEnginePoll >= ENGINE_POLL_INTERVAL_MS) {
        lastEnginePoll = now;
        pollEngineAndVolts();
    }

    if (now - lastTpmsPoll >= TPMS_POLL_INTERVAL_MS) {
        lastTpmsPoll = now;
        pollTpms();
    }
}

bool VgateObdBle::connectToServer() {
    if (g_pClient) {
        if (g_pClient->isConnected()) g_pClient->disconnect();
        NimBLEDevice::deleteClient(g_pClient);
        g_pClient = nullptr;
    }

    g_pClient = NimBLEDevice::createClient();
    g_pClient->setClientCallbacks(&g_clientCallbacks, false);
    g_pClient->setConnectionParams(12, 24, 0, 200);
    g_pClient->setConnectTimeout(6);

    addLog("Connecting to adapter...");
    if (!g_pClient->connect(g_targetAddress, false)) {
        addLog("ERR: connect() failed");
        return false;
    }

    g_writeCandidates.clear();
    g_pCharWrite = nullptr;
    int notifyCount = 0;

    // Discover all services and characteristics on the adapter
    std::vector<NimBLERemoteService*>* services = g_pClient->getServices(true);
    if (!services || services->empty()) {
        addLog("ERR: No GATT services!");
        g_pClient->disconnect();
        return false;
    }

    String svcList = "Svcs:";
    for (auto* svc : *services) {
        String sUuid = shortUuid(svc->getUUID());
        svcList += " " + sUuid;

        // Skip generic GAP/GATT (1800, 1801) and Device Info (180a)
        String sLower = sUuid;
        sLower.toLowerCase();
        if (sLower == "1800" || sLower == "1801" || sLower == "180a") continue;

        std::vector<NimBLERemoteCharacteristic*>* chars = svc->getCharacteristics(true);
        if (!chars) continue;

        for (auto* ch : *chars) {
            String cUuid = shortUuid(ch->getUUID());

            // Subscribe to EVERY Notify / Indicate characteristic so we never miss RX data!
            if (ch->canNotify()) {
                if (ch->subscribe(true, notifyCallback, true)) {
                    addLog("Sub Notify: " + cUuid);
                    notifyCount++;
                } else if (ch->subscribe(true, notifyCallback, false)) {
                    addLog("Sub Notify(NR): " + cUuid);
                    notifyCount++;
                }
            } else if (ch->canIndicate()) {
                if (ch->subscribe(false, notifyCallback, true)) {
                    addLog("Sub Ind: " + cUuid);
                    notifyCount++;
                }
            }

            // Record every writable characteristic as a TX candidate
            if (ch->canWrite() || ch->canWriteNoResponse()) {
                addLog("TX Cand: " + cUuid +
                       (ch->canWriteNoResponse() ? " [WNR]" : "") +
                       (ch->canWrite() ? " [W]" : ""));
                g_writeCandidates.push_back(ch);
            }
        }
    }
    addLog(svcList);

    if (notifyCount == 0 || g_writeCandidates.empty()) {
        addLog("ERR: No UART chars found");
        g_pClient->disconnect();
        return false;
    }

    return true;
}

bool VgateObdBle::probeAndInitElm327() {
    delay(300); // Let adapter settle after GATT discovery

    // Probe each writable characteristic (testing Write-Without-Response first, then Write-With-Response)
    bool foundWorkingChar = false;

    for (int pass = 0; pass < 2 && !foundWorkingChar; pass++) {
        // If pass 1 failed unencrypted, try initiating BLE security encryption (PIN 1234) on pass 2!
        if (pass == 1) {
            addLog("Trying BLE Auth (1234)...");
            g_pClient->secureConnection();
            delay(500);
        }

        for (auto* ch : g_writeCandidates) {
            String cUuid = shortUuid(ch->getUUID());

            // Try both WriteNoResponse (false) and WriteWithResponse (true) if supported
            bool modes[2] = { false, true };
            for (int m = 0; m < 2; m++) {
                bool withResp = modes[m];
                if (!withResp && !ch->canWriteNoResponse()) continue;
                if (withResp && !ch->canWrite()) continue;

                g_pCharWrite = ch;
                g_writeWithResp = withResp;

                addLog("Probe " + cUuid + (withResp ? " [W]:" : " [WNR]:"));
                String resp = sendCommandBlocking("ATI", 1000);
                if (resp.length() == 0 && !g_promptReceived) {
                    resp = sendCommandBlocking("ATZ", 1500);
                }

                if (resp.length() > 0 || g_promptReceived) {
                    String cleanResp = resp;
                    cleanResp.replace("\r", " ");
                    cleanResp.replace("\n", " ");
                    addLog("RX: " + cleanResp);
                    addLog("LOCKED TX: " + cUuid);
                    foundWorkingChar = true;
                    break;
                }
            }
            if (foundWorkingChar) break;
        }
    }

    if (!foundWorkingChar) {
        addLog("No reply from any char!");
        return false;
    }

    // Standard ELM327 initialization sequence
    addLog("TX: ATZ");
    String r = sendCommandBlocking("ATZ", 1500);
    r.replace("\r", " ");
    addLog("RX: " + r);
    delay(200);

    sendCommandBlocking("ATE0", 800);   // Echo Off
    sendCommandBlocking("ATL0", 800);   // Linefeeds Off
    sendCommandBlocking("ATS0", 800);   // Spaces Off
    sendCommandBlocking("ATH0", 800);   // Headers Off

    addLog("TX: ATSP6 (ISO CAN)");
    String spResp = sendCommandBlocking("ATSP6", 1000);
    addLog("RX: " + spResp);

    _currentHeader = "";
    return true;
}

String VgateObdBle::sendCommandBlocking(const char* cmd, uint32_t timeoutMs) {
    if (!g_connected || !g_pCharWrite) return "";

    portENTER_CRITICAL(&g_rxMux);
    g_rxBuffer = "";
    g_promptReceived = false;
    portEXIT_CRITICAL(&g_rxMux);

    String fullCmd = String(cmd) + "\r";
    g_pCharWrite->writeValue((const uint8_t*)fullCmd.c_str(), fullCmd.length(), g_writeWithResp);

    uint32_t start = millis();
    while (!g_promptReceived && (millis() - start < timeoutMs)) {
        if (!g_connected) break;
        delay(5);
    }

    portENTER_CRITICAL(&g_rxMux);
    String clean = g_rxBuffer;
    portEXIT_CRITICAL(&g_rxMux);

    clean.trim();
    return clean;
}

bool VgateObdBle::setHeader(const char* header) {
    if (_currentHeader == header) return true;
    String cmd = String("ATSH") + header;
    String resp = sendCommandBlocking(cmd.c_str(), 800);
    if (resp.indexOf("OK") >= 0) {
        _currentHeader = header;
        return true;
    }
    addLog("ATSH " + String(header) + " -> " + resp);
    return false;
}

void VgateObdBle::pollTpms() {
    if (!setHeader(HEADER_BCM)) return; // Switch to BCM (726)

    uint8_t A = 0, B = 0;
    String resp;

    // Left Front (222813)
    resp = sendCommandBlocking(PID_TPMS_LF, OBD_CMD_TIMEOUT_MS);
    if (parseMode22TwoBytes(resp, "2813", A, B)) {
        float val = calcFordTirePsi(A, B);
        portENTER_CRITICAL(&g_dataMux);
        _data.tireLF = val;
        portEXIT_CRITICAL(&g_dataMux);
        addLog("LF:" + String(val, 1) + " [" + resp.substring(0, 10) + "]");
    } else {
        addLog("LF raw: " + resp);
    }

    // Right Front (222814)
    resp = sendCommandBlocking(PID_TPMS_RF, OBD_CMD_TIMEOUT_MS);
    if (parseMode22TwoBytes(resp, "2814", A, B)) {
        float val = calcFordTirePsi(A, B);
        portENTER_CRITICAL(&g_dataMux);
        _data.tireRF = val;
        portEXIT_CRITICAL(&g_dataMux);
    }

    // Right Rear (222815)
    resp = sendCommandBlocking(PID_TPMS_RR, OBD_CMD_TIMEOUT_MS);
    if (parseMode22TwoBytes(resp, "2815", A, B)) {
        float val = calcFordTirePsi(A, B);
        portENTER_CRITICAL(&g_dataMux);
        _data.tireRR = val;
        portEXIT_CRITICAL(&g_dataMux);
    }

    // Left Rear (222816)
    resp = sendCommandBlocking(PID_TPMS_LR, OBD_CMD_TIMEOUT_MS);
    if (parseMode22TwoBytes(resp, "2816", A, B)) {
        float val = calcFordTirePsi(A, B);
        portENTER_CRITICAL(&g_dataMux);
        _data.tireLR = val;
        portEXIT_CRITICAL(&g_dataMux);
    }

    portENTER_CRITICAL(&g_dataMux);
    _data.lastUpdateMs = millis();
    portEXIT_CRITICAL(&g_dataMux);
}

void VgateObdBle::pollEngineAndVolts() {
    String vResp = sendCommandBlocking(CMD_BATTERY_VOLT, 800);
    vResp.replace("V", "");
    vResp.replace("v", "");
    float v = vResp.toFloat();
    if (v > 5.0f && v < 20.0f) {
        portENTER_CRITICAL(&g_dataMux);
        _data.batteryVolt = v;
        portEXIT_CRITICAL(&g_dataMux);
    }

    if (!setHeader(HEADER_PCM)) return;

    uint8_t A = 0, B = 0;
    String resp;

    // Transmission Fluid Temp (221E1C)
    resp = sendCommandBlocking(PID_TRANS_TEMP, OBD_CMD_TIMEOUT_MS);
    if (parseMode22TwoBytes(resp, "1E1C", A, B)) {
        float val = calcFordTransTempF(A, B);
        portENTER_CRITICAL(&g_dataMux);
        _data.transTempF = val;
        portEXIT_CRITICAL(&g_dataMux);
    }

    // Engine Coolant Temp (0105)
    resp = sendCommandBlocking(PID_COOLANT_TEMP, OBD_CMD_TIMEOUT_MS);
    if (parseMode01OneByte(resp, "05", A)) {
        float val = calcStandardTempF(A);
        portENTER_CRITICAL(&g_dataMux);
        _data.coolantTempF = val;
        portEXIT_CRITICAL(&g_dataMux);
    }

    // Intake Air Temp (010F)
    resp = sendCommandBlocking(PID_INTAKE_TEMP, OBD_CMD_TIMEOUT_MS);
    if (parseMode01OneByte(resp, "0F", A)) {
        float val = calcStandardTempF(A);
        portENTER_CRITICAL(&g_dataMux);
        _data.intakeTempF = val;
        portEXIT_CRITICAL(&g_dataMux);
    }

    portENTER_CRITICAL(&g_dataMux);
    _data.lastUpdateMs = millis();
    portEXIT_CRITICAL(&g_dataMux);
}

bool VgateObdBle::parseMode22TwoBytes(const String& rawResp, const char* pidHex, uint8_t& outA, uint8_t& outB) {
    String clean = rawResp;
    clean.replace(" ", "");
    clean.replace("\r", "");
    clean.replace("\n", "");
    clean.toUpperCase();

    String prefix = String("62") + pidHex;
    prefix.toUpperCase();

    int idx = clean.indexOf(prefix);
    if (idx < 0 || (idx + prefix.length() + 4) > clean.length()) {
        return false;
    }

    int dataStart = idx + prefix.length();
    String hexA = clean.substring(dataStart, dataStart + 2);
    String hexB = clean.substring(dataStart + 2, dataStart + 4);

    outA = static_cast<uint8_t>(strtoul(hexA.c_str(), nullptr, 16));
    outB = static_cast<uint8_t>(strtoul(hexB.c_str(), nullptr, 16));
    return true;
}

bool VgateObdBle::parseMode01OneByte(const String& rawResp, const char* pidHex, uint8_t& outA) {
    String clean = rawResp;
    clean.replace(" ", "");
    clean.replace("\r", "");
    clean.replace("\n", "");
    clean.toUpperCase();

    String prefix = String("41") + pidHex;
    prefix.toUpperCase();

    int idx = clean.indexOf(prefix);
    if (idx < 0 || (idx + prefix.length() + 2) > clean.length()) {
        return false;
    }

    int dataStart = idx + prefix.length();
    String hexA = clean.substring(dataStart, dataStart + 2);
    outA = static_cast<uint8_t>(strtoul(hexA.c_str(), nullptr, 16));
    return true;
}

float VgateObdBle::calcFordTirePsi(uint8_t A, uint8_t B) {
    uint16_t raw = (static_cast<uint16_t>(A) << 8) | B;
    if (raw == 0xFFFF || raw == 0) return -1.0f;
    return (((256.0f * A) + B) / 3.0f + (22.0f / 3.0f)) * 0.145038f;
}

float VgateObdBle::calcFordTransTempF(uint8_t A, uint8_t B) {
    int16_t signedRaw = static_cast<int16_t>((static_cast<uint16_t>(A) << 8) | B);
    return (signedRaw * (9.0f / 80.0f)) + 32.0f;
}

float VgateObdBle::calcStandardTempF(uint8_t A) {
    return ((static_cast<float>(A) - 40.0f) * 1.8f) + 32.0f;
}

void VgateObdBle::updateDemoValues() {
    float t = millis() / 1000.0f;
    portENTER_CRITICAL(&g_dataMux);
    _data.tireLF = 35.2f + 0.4f * sinf(t * 0.3f);
    _data.tireRF = 34.8f + 0.4f * cosf(t * 0.25f);
    _data.tireLR = 35.0f + 0.3f * sinf(t * 0.2f);
    _data.tireRR = (fmodf(t, 24.0f) > 16.0f) ? 28.4f : (34.9f + 0.3f * cosf(t * 0.35f));

    _data.transTempF   = 188.0f + 6.0f * sinf(t * 0.15f);
    _data.coolantTempF = 196.0f + 4.0f * cosf(t * 0.12f);
    _data.intakeTempF  = 78.0f  + 3.0f * sinf(t * 0.1f);
    _data.batteryVolt  = 14.2f  + 0.1f * sinf(t * 0.8f);
    _data.lastUpdateMs = millis();
    portEXIT_CRITICAL(&g_dataMux);
}
