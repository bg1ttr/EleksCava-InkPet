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
        // Task progress (from TodoWrite tool)
        bool hasTasks;
        uint16_t tasksDone;
        uint16_t tasksRunning;
        uint16_t tasksPending;
    };
    void showAgentState(const AgentDisplayInfo& info);

    // BLE / Claude desktop buddy HUD: uses snapshot fields the upstream
    // protocol provides directly (msg, tokens_today, entries) which are
    // more informative than the derived state+tool+file shown by the
    // regular agent state card.
    struct BuddyHudInfo {
        const char* stateName;      // display name ("Working", "Attention", ...)
        const uint8_t* pixelArt;    // 48x48 1-bit XBM
        const char* msg;            // one-line upstream summary
        uint32_t tokensToday;       // tokens since local midnight
        uint8_t  nEntries;
        const char* entries[4];     // newest first, ≤92 chars each
        bool secured;               // BLE link is bonded + encrypted
        const char* deviceName;     // "Claude-XXXX"
    };
    void showBuddyHUD(const BuddyHudInfo& info);

    void showPermissionRequest(const char* agentName, const char* tool,
                               const char* file);
    void showDeviceInfo(const String& ip, const String& mac,
                        int rssi, float battVolt, int battPct,
                        size_t freeHeap);
    void showTimeMode(const String& time, const String& date);

    // Power management
    void hibernate();  // Power off e-paper panel (for idle state)
    void wake();       // Reinitialize panel after hibernate
    bool isHibernated() const { return _hibernated; }

    // Force next render to be a full refresh (for periodic ghosting prevention)
    void requestFullRefresh() { _forceFullRefresh = true; }

    // Utilities
    GxEPD2_BW<GxEPD2_290_T94, DISP_HEIGHT>* getDisplay() { return _display; }
    U8G2_FOR_ADAFRUIT_GFX* getFonts() { return &_u8g2; }
    SemaphoreHandle_t getMutex() { return _mutex; }
private:
    DisplayManager();
    static DisplayManager* _instance;

    GxEPD2_BW<GxEPD2_290_T94, DISP_HEIGHT>* _display;
    U8G2_FOR_ADAFRUIT_GFX _u8g2;
    SemaphoreHandle_t _mutex;
    bool _initialized;
    bool _hibernated;         // Panel powered off
    bool _forceFullRefresh;   // Force next draw regardless of content cache
    uint8_t _lastScreenType;  // Track screen type for anti-ghosting deepClean

    // Content fingerprint — skip full refresh if nothing meaningful changed
    struct ContentCache {
        char stateName[16];
        char agentName[32];
        char tool[20];
        char file[32];
        int  activeSessions;
        bool hasTasks;
        uint16_t tasksDone, tasksRunning, tasksPending;
    };
    ContentCache _lastContent;
    bool _contentCacheValid;

    bool _hasContentChanged(const AgentDisplayInfo& info) const;
    void _updateContentCache(const AgentDisplayInfo& info);

    // Buddy HUD fingerprint — snapshot-driven so it fires every ~10s even
    // when nothing changed; dedupe to protect e-paper lifetime.
    struct BuddyHudCache {
        char stateName[16];
        char msg[40];
        uint32_t tokensToday;
        uint8_t  nEntries;
        char entries[3][92];
        bool secured;
        bool valid;
    };
    BuddyHudCache _lastHud;
    bool _hudChanged(const BuddyHudInfo& info) const;
    void _updateHudCache(const BuddyHudInfo& info);

    void drawCenteredText(const char* text, int y, const uint8_t* font);
    void drawTextAt(const char* text, int x, int y, const uint8_t* font);
    void deepClean();  // Anti-ghosting: black->white full refresh cycle
};
