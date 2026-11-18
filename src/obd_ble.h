#pragma once
#include <Arduino.h>

enum class ObdState {
    DISCONNECTED,
    SCANNING,
    CONNECTING,
    INITIALIZING_ELM,
    READY,
    DEMO_MODE
};

struct FlexVehicleData {
    // 4 Individual Tire Pressures (PSI) from BCM (Header 726)
    float tireLF = -1.0f;   // Left Front  (PID 222813)
    float tireRF = -1.0f;   // Right Front (PID 222814)
    float tireLR = -1.0f;   // Left Rear   (PID 222816)
    float tireRR = -1.0f;   // Right Rear  (PID 222815)

    // Powertrain & Electrical stats from PCM (Header 7E0) & Adapter
    float transTempF   = -999.0f; // 6F50 Transmission Fluid Temp (PID 221E1C)
    float coolantTempF = -999.0f; // Engine Coolant Temp (PID 0105)
    float intakeTempF  = -999.0f; // Intake Air Temp (PID 010F)
    float batteryVolt  = -1.0f;   // Battery / Alternator Voltage (ATRV)

    uint32_t lastUpdateMs = 0;
    ObdState state = ObdState::DISCONNECTED;
    char statusText[24] = "Starting...";
};

class VgateObdBle {
public:
    static constexpr int MAX_LOG_LINES = 11;

    void begin(bool startInDemo = false);
    void update();
    void toggleDemoMode();
    bool isDemoMode() const { return _demoMode; }
    FlexVehicleData getData() const;

    // Access verbose debug log lines for on-screen troubleshooting
    void getLogLines(String outLines[MAX_LOG_LINES], int& outCount) const;
    static void addLog(const String& line);

private:
    FlexVehicleData _data;
    bool _demoMode = false;
    String _currentHeader = "";

    void setStatus(ObdState state, const char* text);
    bool connectToServer();
    bool probeAndInitElm327();
    String sendCommandBlocking(const char* cmd, uint32_t timeoutMs = 1200);
    bool setHeader(const char* header);

    void pollTpms();
    void pollEngineAndVolts();
    void updateDemoValues();

    static bool parseMode22TwoBytes(const String& resp, const char* pidHex, uint8_t& outA, uint8_t& outB);
    static bool parseMode01OneByte(const String& resp, const char* pidHex, uint8_t& outA);
    static float calcFordTirePsi(uint8_t A, uint8_t B);
    static float calcFordTransTempF(uint8_t A, uint8_t B);
    static float calcStandardTempF(uint8_t A);
};
