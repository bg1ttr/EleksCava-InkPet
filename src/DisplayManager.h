#pragma once
#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "config.h"

class DisplayManager {
public:
    static DisplayManager* getInstance();
    void init(GxEPD2_BW<GxEPD2_290_T94, DISP_HEIGHT>* d);

    // Core display operations
    void clearScreen();
    void fullRefresh();
    void partialRefresh();

    // High-level display methods
    void showWelcome(const String& version, const String& mac);
    void showAPMode(const String& ssid, const String& ip);
    void showWiFiConnecting(const String& ssid);
    void showWiFiError(const String& error);
    void showMessage(const String& msg);
    void showError(const String& error);

    // Agent state display (rich info version inspired by Vibe Island)
    struct AgentDisplayInfo {
        const char* stateName;
        const char* agentName;
        const char* tool;
        const char* file;
        const uint8_t* pixelArt;
        unsigned long elapsedMs;    // Session elapsed time
        uint16_t toolCalls;         // Total tool invocations
        uint16_t reads;
        uint16_t writes;
        uint16_t edits;
        uint16_t bashes;
        int activeSessions;
    };
    void showAgentState(const AgentDisplayInfo& info);
    void showPermissionRequest(const char* agentName, const char* tool,
                               const char* file);
    void showDeviceInfo(const String& ip, const String& mac,
                        int rssi, float battVolt, int battPct,
                        size_t freeHeap);
    void showTimeMode(const String& time, const String& date);

    // Utilities
    GxEPD2_BW<GxEPD2_290_T94, DISP_HEIGHT>* getDisplay() { return _display; }
    U8G2_FOR_ADAFRUIT_GFX* getFonts() { return &_u8g2; }
    SemaphoreHandle_t getMutex() { return _mutex; }
    void incrementPartialCount();
    bool needsFullRefresh() const;

private:
    DisplayManager();
    static DisplayManager* _instance;

    GxEPD2_BW<GxEPD2_290_T94, DISP_HEIGHT>* _display;
    U8G2_FOR_ADAFRUIT_GFX _u8g2;
    SemaphoreHandle_t _mutex;
    int _partialCount;
    bool _initialized;
    uint8_t _lastScreenType;  // Track screen type for ghosting prevention

    void drawCenteredText(const char* text, int y, const uint8_t* font);
    void drawTextAt(const char* text, int x, int y, const uint8_t* font);
    void deepClean();  // Anti-ghosting: black->white full refresh cycle
};
