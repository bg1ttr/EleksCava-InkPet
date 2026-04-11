// =============================================================================
// InksPet - Main Firmware Entry Point
// ESP32 e-paper AI agent companion device
// =============================================================================

#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "version.h"
#include "Logger.h"
#include "NVSManager.h"
#include "MemoryMonitor.h"
#include "ConfigManager.h"
#include "DisplayManager.h"
#include "RGBLed.h"
#include "KeyManager.h"
#include "BuzzerManager.h"
#include "BatteryManager.h"
#include "WiFiManager.h"
#include "TimeManager.h"
#include "InksPetWebServer.h"
#include "AgentStateManager.h"
#include "PermissionManager.h"
#include "PixelArt.h"

static const char* TAG = "MAIN";

// =============================================================================
// Display mode cycling
// =============================================================================
enum class DisplayMode : uint8_t {
    AGENT_STATE,
    DEVICE_INFO,
    TIME_MODE
};

static DisplayMode currentDisplayMode = DisplayMode::AGENT_STATE;
static unsigned long lastTimeDisplayUpdate = 0;
static const unsigned long TIME_DISPLAY_INTERVAL_MS = 60000;  // 1 minute
static unsigned long lastAgentDisplayRefresh = 0;
static const unsigned long AGENT_DISPLAY_REFRESH_MS = 10000;  // 10 seconds

// Periodic full refresh to prevent e-paper ghosting + hibernate for idle
static unsigned long lastFullRefreshMs = 0;
static const unsigned long FULL_REFRESH_INTERVAL_MS = 10UL * 60UL * 1000UL;  // 10 minutes
static unsigned long idleEnteredMs = 0;
static const unsigned long HIBERNATE_AFTER_IDLE_MS = 30UL * 1000UL;  // hibernate 30s after entering idle
static bool displayHibernatedForIdle = false;

// =============================================================================
// GxEPD2 display object (allocated in setup to avoid global constructor crash)
// =============================================================================
static GxEPD2_BW<GxEPD2_290_T94, DISP_HEIGHT>* epd = nullptr;

// =============================================================================
// Singleton accessors (cached for readability)
// =============================================================================
static NVSManager*        nvs          = nullptr;
static ConfigManager*     config       = nullptr;
static DisplayManager*    display      = nullptr;
static RGBLed*            led          = nullptr;
static KeyManager*        keys         = nullptr;
static BuzzerManager*     buzzer       = nullptr;
static BatteryManager*    battery      = nullptr;
static WiFiManager*       wifi         = nullptr;
static TimeManager*       timeMgr      = nullptr;
static InksPetWebServer*   webServer    = nullptr;
static AgentStateManager* agentMgr     = nullptr;
static PermissionManager* permMgr      = nullptr;
static MemoryMonitor*     memMonitor   = nullptr;

// =============================================================================
// Forward declarations
// =============================================================================
static void onAgentStateChange(AgentState state, const AgentSession* session);
static void onPermissionResponse(const String& sessionId, const String& action);
static void handleKeyEvent(KeyEvent event);
static void handleKeyEventAPMode(KeyEvent event);
static void updateDisplayForCurrentMode();
static void refreshAgentStateDisplay();
static void mapStateToLed(AgentState state);
static void enterAPMode();
static bool initNVS();
static bool initLittleFS();
static bool initDisplay();
static bool connectWiFi();
static void initPostWiFi();

// =============================================================================
// setup()
// =============================================================================
void setup() {
    // ---- 1. Serial init ----
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n===== InksPet Boot =====");
    Serial.flush();
    LOG_INFO(TAG, "=== %s v%s starting ===", FIRMWARE_NAME, VERSION);
    LOG_INFO(TAG, "Platform: %s", HARDWARE_PLATFORM);
    LOG_INFO(TAG, "Free heap at start: %u", ESP.getFreeHeap());

    // ---- 2. Watchdog ----
    // Skip esp_task_wdt_add to avoid early WDT reset during long init

    // ---- 3. NVS init with recovery ----
    if (!initNVS()) {
        LOG_ERROR(TAG, "NVS init failed, continuing with defaults");
    }

    // ---- 4. LittleFS mount ----
    if (!initLittleFS()) {
        LOG_ERROR(TAG, "LittleFS mount failed, web UI unavailable");
    }

    // ---- 5. Battery monitor init ----
    battery = BatteryManager::getInstance();
    battery->init();
    battery->update();
    LOG_INFO(TAG, "Battery: %.2fV %d%% charging=%d",
             battery->getVoltage(), battery->getPercentage(), battery->isCharging());

    // ---- 6. Display init ----
    if (!initDisplay()) {
        LOG_ERROR(TAG, "Display init failed, halting");
        // Keep watchdog alive and blink LED to indicate error
        led = RGBLed::getInstance();
        led->init();
        led->setEffect(LedEffect::FAST_FLASH, LedColors::RED);
        while (true) {
            esp_task_wdt_reset();
            led->update();
            delay(50);
        }
    }

    // ---- 7. LED init (low brightness for boot, rainbow) ----
    led = RGBLed::getInstance();
    led->init();
    led->setBrightness(10);
    led->startRainbow();
    LOG_INFO(TAG, "RGB LED initialized");

    // ---- 8. Show welcome screen (hold 1 second) ----
    String mac = WiFi.macAddress();
    display->showWelcome(VERSION, mac);
    delay(1000);

    // ---- 9. Buzzer init + welcome melody ----
    buzzer = BuzzerManager::getInstance();
    buzzer->init();
    config = ConfigManager::getInstance();
    // Pre-load config early so buzzer gets correct enabled state
    config->loadConfig();
    buzzer->setEnabled(config->getBuzzerEnabled());
    buzzer->playWelcome();
    LOG_INFO(TAG, "Buzzer initialized (enabled=%d)", buzzer->isEnabled());

    // ---- 10. Button init ----
    keys = KeyManager::getInstance();
    keys->init();
    LOG_INFO(TAG, "Buttons initialized");

    // ---- 11. Load config (already loaded above, apply remaining settings) ----
    led->setBrightnessLevel(config->getLedBrightness());
    LOG_INFO(TAG, "Config loaded (DND=%d, brightness=%s)",
             config->getDndMode(), config->getLedBrightness().c_str());

    // ---- 12. Check WiFi config ----
    if (!WiFiManager::hasWiFiConfig()) {
        LOG_INFO(TAG, "No WiFi config found, entering AP mode");
        enterAPMode();
        return;
    }

    // Has WiFi config -> attempt connection
    display->showWiFiConnecting(wifi ? wifi->getSSID() : "saved network");
    setCpuFrequencyMhz(80);
    LOG_INFO(TAG, "CPU frequency reduced to 80MHz for WiFi connect");

    // ---- 13. WiFi connect ----
    if (!connectWiFi()) {
        LOG_WARNING(TAG, "WiFi connect failed, entering AP mode");
        setCpuFrequencyMhz(240);
        enterAPMode();
        return;
    }

    // Restore full CPU speed
    setCpuFrequencyMhz(240);
    LOG_INFO(TAG, "CPU frequency restored to 240MHz");

    // ---- 14-20. Post-WiFi initialization ----
    initPostWiFi();

    // Initialize anti-ghosting timer baseline
    lastFullRefreshMs = millis();

    LOG_INFO(TAG, "=== %s ready! ===", FIRMWARE_NAME);
    LOG_INFO(TAG, "Free heap: %u bytes", ESP.getFreeHeap());
}

// =============================================================================
// loop()
// =============================================================================
void loop() {
    // ---- 1. Watchdog feed ----
    // esp_task_wdt_reset();  // Disabled: not registered in this boot

    // ---- 2. AP mode loop shortcut ----
    if (wifi && wifi->isAPMode()) {
        wifi->loop();
        led->update();
        buzzer->update();

        KeyEvent event = keys->poll();
        if (event != KeyEvent::NONE) {
            handleKeyEventAPMode(event);
        }

        delay(10);
        return;
    }

    // ---- 3. LED update (animation) ----
    led->update();

    // ---- 4. Buzzer update (melody playback) ----
    buzzer->update();

    // ---- 5. Battery update ----
    battery->update();

    // ---- 6. Memory monitor update ----
    memMonitor->update();

    // ---- 7. Handle key input ----
    KeyEvent event = keys->poll();
    if (event != KeyEvent::NONE) {
        handleKeyEvent(event);
    }

    // ---- 8. Permission timeout check ----
    permMgr->update();

    // ---- 9. Agent session cleanup (stale sessions) ----
    agentMgr->cleanupStaleSessions();

    // ---- 10. Time manager update (hourly NTP re-sync) ----
    timeMgr->update();

    // ---- 11a. Idle hibernate + periodic full refresh (e-paper lifespan) ----
    {
        AgentState curState = agentMgr->getCurrentState();
        bool isIdle = (curState == AgentState::SLEEPING || curState == AgentState::IDLE);
        unsigned long now = millis();

        if (isIdle) {
            // Track when we entered idle
            if (idleEnteredMs == 0) idleEnteredMs = now;

            // After HIBERNATE_AFTER_IDLE_MS of idle, power off the panel
            if (!displayHibernatedForIdle &&
                (now - idleEnteredMs >= HIBERNATE_AFTER_IDLE_MS)) {
                LOG_INFO(TAG, "Idle for %lus, hibernating display",
                         (now - idleEnteredMs) / 1000);
                display->hibernate();
                displayHibernatedForIdle = true;
            }
        } else {
            // Not idle — reset idle timer
            idleEnteredMs = 0;
            displayHibernatedForIdle = false;

            // Periodic full refresh every 10min to prevent ghosting
            // Only when active and not hibernated
            if (!display->isHibernated() &&
                (now - lastFullRefreshMs >= FULL_REFRESH_INTERVAL_MS)) {
                LOG_INFO(TAG, "10-minute full refresh (anti-ghosting)");
                lastFullRefreshMs = now;
                display->requestFullRefresh();
                updateDisplayForCurrentMode();
            }
        }
    }

    // ---- 11b. Update time display if in time mode (every minute) ----
    if (currentDisplayMode == DisplayMode::TIME_MODE) {
        unsigned long now = millis();
        if (now - lastTimeDisplayUpdate >= TIME_DISPLAY_INTERVAL_MS) {
            lastTimeDisplayUpdate = now;
            if (timeMgr->isTimeValid()) {
                display->showTimeMode(
                    timeMgr->getFormattedTime(),
                    timeMgr->getFormattedDate()
                );
            }
        }
    }

    // ---- 12. Yield to RTOS ----
    delay(10);
}

// =============================================================================
// Agent state change callback
// =============================================================================
static void onAgentStateChange(AgentState state, const AgentSession* session) {
    LOG_INFO(TAG, "State change -> %s (agent=%s)",
             AgentStateManager::stateToString(state),
             session ? session->agentName.c_str() : "none");

    // Update LED effect + color based on state
    mapStateToLed(state);

    // Play buzzer sounds for notable transitions
    if (!config->getDndMode()) {
        switch (state) {
            case AgentState::PERMISSION:
                buzzer->playPermissionAlert();
                break;
            case AgentState::ERROR:
                buzzer->playError();
                break;
            case AgentState::COMPLETED:
                buzzer->playTaskComplete();
                break;
            default:
                break;
        }
    }

    // Auto-switch to agent state display when any agent activity occurs
    if (currentDisplayMode != DisplayMode::AGENT_STATE) {
        if (state != AgentState::SLEEPING && state != AgentState::IDLE) {
            LOG_INFO(TAG, "Auto-switching to agent state display");
            currentDisplayMode = DisplayMode::AGENT_STATE;
        } else {
            return;
        }
    }

    // Render the appropriate display
    if (state == AgentState::PERMISSION && permMgr->hasPending()) {
        display->showPermissionRequest(
            permMgr->getCurrentAgent(),
            permMgr->getCurrentTool(),
            permMgr->getCurrentFile()
        );
    } else if (session) {
        const char* sn = AgentStateManager::stateToString(state);
        DisplayManager::AgentDisplayInfo info = {};
        info.stateName = AgentStateManager::stateToDisplayName(state);
        info.agentName = session->agentName.c_str();
        info.tool = session->tool.c_str();
        info.file = session->file.c_str();
        info.pixelArt = getPixelArtForState(sn);
        info.elapsedMs = millis() - session->sessionStart;
        info.toolCalls = session->toolCalls;
        info.reads = session->reads;
        info.writes = session->writes;
        info.edits = session->edits;
        info.bashes = session->bashes;
        info.activeSessions = agentMgr->getActiveSessionCount();
        display->showAgentState(info);
    } else {
        DisplayManager::AgentDisplayInfo info = {};
        info.stateName = "Idle";
        info.agentName = "";
        info.tool = "";
        info.file = "";
        info.pixelArt = getPixelArtForState("idle");
        info.elapsedMs = 0;
        info.activeSessions = 0;
        display->showAgentState(info);
    }
}

// =============================================================================
// Permission response callback
// =============================================================================
static void onPermissionResponse(const String& sessionId, const String& action) {
    LOG_INFO(TAG, "Permission response: session=%s action=%s",
             sessionId.c_str(), action.c_str());

    // Forward response to the agent state manager for webhook relay
    agentMgr->respondToPermission(sessionId, action);

    // Play confirmation sound
    if (!config->getDndMode()) {
        if (action == "deny") {
            buzzer->playError();
        } else {
            buzzer->playConnect();
        }
    }
}

// =============================================================================
// Key event handler (normal mode)
// =============================================================================
static void handleKeyEvent(KeyEvent event) {
    // If permission is pending, buttons are dedicated to permission response
    if (permMgr->hasPending()) {
        switch (event) {
            case KeyEvent::KEY_A_SHORT:
                LOG_INFO(TAG, "Permission: ALLOW");
                permMgr->handleAllow();
                break;
            case KeyEvent::KEY_B_SHORT:
                LOG_INFO(TAG, "Permission: ALWAYS ALLOW");
                permMgr->handleAlwaysAllow();
                break;
            case KeyEvent::KEY_C_SHORT:
                LOG_INFO(TAG, "Permission: DENY");
                permMgr->handleDeny();
                break;
            default:
                break;
        }
        return;
    }

    // Normal mode key handling
    switch (event) {
        case KeyEvent::KEY_A_SHORT: {
            // Cycle display mode: AGENT_STATE -> DEVICE_INFO -> TIME_MODE -> ...
            switch (currentDisplayMode) {
                case DisplayMode::AGENT_STATE:
                    currentDisplayMode = DisplayMode::DEVICE_INFO;
                    break;
                case DisplayMode::DEVICE_INFO:
                    currentDisplayMode = DisplayMode::TIME_MODE;
                    break;
                case DisplayMode::TIME_MODE:
                    currentDisplayMode = DisplayMode::AGENT_STATE;
                    break;
            }
            LOG_INFO(TAG, "Display mode -> %d", (int)currentDisplayMode);
            updateDisplayForCurrentMode();
            break;
        }

        case KeyEvent::KEY_B_LONG: {
            // Toggle DND mode
            bool dnd = !config->getDndMode();
            config->setDndMode(dnd);
            config->saveConfig();
            LOG_INFO(TAG, "DND mode %s", dnd ? "ON" : "OFF");

            if (dnd) {
                led->setEffect(LedEffect::FADE_ONCE, LedColors::PURPLE);
            } else {
                // Restore LED to current agent state
                mapStateToLed(agentMgr->getCurrentState());
            }

            // Brief on-screen notification via message
            display->showMessage(dnd ? "DND Mode ON" : "DND Mode OFF");
            // Restore display after a short period on next loop cycle
            delay(800);
            updateDisplayForCurrentMode();
            break;
        }

        case KeyEvent::KEY_C_SHORT:
            // Reserved for future use
            LOG_DEBUG(TAG, "Key C short press (reserved)");
            break;

        case KeyEvent::COMBO_AC: {
            // Enter AP mode for reconfiguration
            LOG_WARNING(TAG, "COMBO_AC pressed, entering AP mode");
            enterAPMode();
            break;
        }

        default:
            break;
    }
}

// =============================================================================
// Key event handler (AP mode)
// =============================================================================
static void handleKeyEventAPMode(KeyEvent event) {
    // In AP mode, only COMBO_AC restarts to retry normal connection
    if (event == KeyEvent::COMBO_AC) {
        LOG_INFO(TAG, "Restart requested from AP mode");
        ESP.restart();
    }
}

// =============================================================================
// Update display for current mode
// =============================================================================
static void updateDisplayForCurrentMode() {
    switch (currentDisplayMode) {
        case DisplayMode::AGENT_STATE:
            refreshAgentStateDisplay();
            break;

        case DisplayMode::DEVICE_INFO:
            display->showDeviceInfo(
                wifi->getIP(),
                wifi->getMAC(),
                wifi->getRSSI(),
                battery->getVoltage(),
                battery->getPercentage(),
                memMonitor->getFreeHeap()
            );
            break;

        case DisplayMode::TIME_MODE:
            if (timeMgr->isTimeValid()) {
                display->showTimeMode(
                    timeMgr->getFormattedTime(),
                    timeMgr->getFormattedDate()
                );
                lastTimeDisplayUpdate = millis();
            } else {
                display->showMessage("Time not synced");
            }
            break;
    }
}

// =============================================================================
// Refresh agent state on display
// =============================================================================
static void refreshAgentStateDisplay() {
    AgentState state = agentMgr->getCurrentState();
    const AgentSession* session = agentMgr->getCurrentSession();

    if (state == AgentState::PERMISSION && permMgr->hasPending()) {
        display->showPermissionRequest(
            permMgr->getCurrentAgent(),
            permMgr->getCurrentTool(),
            permMgr->getCurrentFile()
        );
    } else if (session) {
        const char* sn = AgentStateManager::stateToString(state);
        DisplayManager::AgentDisplayInfo info = {};
        info.stateName = AgentStateManager::stateToDisplayName(state);
        info.agentName = session->agentName.c_str();
        info.tool = session->tool.c_str();
        info.file = session->file.c_str();
        info.pixelArt = getPixelArtForState(sn);
        info.elapsedMs = millis() - session->sessionStart;
        info.toolCalls = session->toolCalls;
        info.reads = session->reads;
        info.writes = session->writes;
        info.edits = session->edits;
        info.bashes = session->bashes;
        info.activeSessions = agentMgr->getActiveSessionCount();
        display->showAgentState(info);
    } else {
        DisplayManager::AgentDisplayInfo info = {};
        info.stateName = "Idle";
        info.agentName = "";
        info.tool = "";
        info.file = "";
        info.pixelArt = getPixelArtForState("idle");
        info.elapsedMs = 0;
        info.activeSessions = 0;
        display->showAgentState(info);
    }
}

// =============================================================================
// Map AgentState to LED effect and color
// =============================================================================
// LED color logic: blue=thinking, green=working, red=error, yellow=action needed
static void mapStateToLed(AgentState state) {
    switch (state) {
        case AgentState::SLEEPING:
            led->off();                                              // Silent, device looks off
            break;
        case AgentState::IDLE:
            led->setEffect(LedEffect::SOLID, LedColors::WHITE_DIM); // Dim white = standby
            break;
        case AgentState::COMPLETED:
            led->setEffect(LedEffect::FADE_ONCE, LedColors::GREEN); // Green pulse = done!
            break;
        case AgentState::CARRYING:
            led->setEffect(LedEffect::SOLID, LedColors::ORANGE);    // Orange = transporting
            break;
        case AgentState::SWEEPING:
            led->setEffect(LedEffect::BREATHING, LedColors::CYAN);  // Cyan = cleanup
            break;
        case AgentState::JUGGLING:
            led->setEffect(LedEffect::BREATHING, LedColors::PURPLE);// Purple breathing = 1 subagent
            break;
        case AgentState::CONDUCTING:
            led->setEffect(LedEffect::SOLID, LedColors::PURPLE);    // Purple solid = orchestrating
            break;
        case AgentState::THINKING:
            led->setEffect(LedEffect::BREATHING, LedColors::BLUE);  // Blue = processing
            break;
        case AgentState::WORKING:
            led->setEffect(LedEffect::SOLID, LedColors::GREEN);     // Green = productive
            break;
        case AgentState::ERROR:
            led->setEffect(LedEffect::FAST_FLASH, LedColors::RED);  // Red flash = error
            break;
        case AgentState::PERMISSION:
            led->setEffect(LedEffect::FLASH, LedColors::YELLOW);    // Yellow flash = action needed
            break;
    }
}

// =============================================================================
// Enter AP mode (shared between boot and runtime)
// =============================================================================
// Captive portal HTML - redirects to config page
static const char CAPTIVE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta http-equiv="refresh" content="0;url=http://192.168.4.1/">
<script>window.location.href="http://192.168.4.1/";</script>
<title>InksPet Setup</title>
</head><body style="background:#0d1117;color:#e6edf3;font-family:sans-serif;text-align:center;padding:40px">
<h2>InksPet WiFi Setup</h2>
<p>Redirecting to configuration page...</p>
<p><a href="http://192.168.4.1/" style="color:#76B900">Click here if not redirected</a></p>
</body></html>
)rawliteral";

static void setupCaptivePortalRoutes(AsyncWebServer* server) {
    // Apple captive portal detection (iOS/macOS)
    server->on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest* request) {
        LOG_INFO(TAG, "Apple captive portal probe");
        request->redirect("http://192.168.4.1/");
    });
    server->on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("http://192.168.4.1/");
    });

    // Android captive portal detection
    server->on("/generate_204", HTTP_GET, [](AsyncWebServerRequest* request) {
        LOG_INFO(TAG, "Android captive portal probe");
        request->redirect("http://192.168.4.1/");
    });
    server->on("/gen_204", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("http://192.168.4.1/");
    });

    // Windows captive portal detection
    server->on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest* request) {
        LOG_INFO(TAG, "Windows captive portal probe");
        request->redirect("http://192.168.4.1/");
    });
    server->on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("http://192.168.4.1/");
    });
    server->on("/fwlink", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("http://192.168.4.1/");
    });

    // Firefox captive portal detection
    server->on("/canonical.html", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->redirect("http://192.168.4.1/");
    });
    server->on("/success.txt", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", "");  // Empty = not connected = show portal
    });

    // Catch-all: any unknown host or path -> redirect to config page
    server->onNotFound([](AsyncWebServerRequest* request) {
        String host = request->host();
        String url = request->url();

        // Let API and static file 404s through normally in STA mode
        if (WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA) {
            if (url.startsWith("/api/")) {
                request->send(404, "application/json", "{\"error\":\"not found\"}");
            } else {
                request->send(404, "text/plain", "Not found");
            }
            return;
        }

        // In AP mode: if requesting our own IP and a known file extension, let it 404 normally
        if (url.endsWith(".css") || url.endsWith(".js") || url.endsWith(".ico") ||
            url.startsWith("/api/")) {
            request->send(404, "text/plain", "Not found");
            return;
        }

        // Everything else -> captive portal redirect page
        LOG_INFO(TAG, "Captive redirect: %s%s", host.c_str(), url.c_str());
        AsyncWebServerResponse* response = request->beginResponse_P(200, "text/html", CAPTIVE_HTML);
        response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        request->send(response);
    });

    LOG_INFO(TAG, "Captive portal routes configured");
}

static void enterAPMode() {
    wifi = WiFiManager::getInstance();
    wifi->startAPMode();

    String apSSID = wifi->getAPSSID();
    String apIP = wifi->getIP();
    LOG_INFO(TAG, "AP mode active: SSID=%s IP=%s", apSSID.c_str(), apIP.c_str());

    display->showAPMode(apSSID, apIP);

    // Start web server for configuration
    webServer = InksPetWebServer::getInstance();
    if (webServer->begin()) {
        LOG_INFO(TAG, "Web server started in AP mode");

        // Add captive portal routes on top of the web server
        // This overrides the default onNotFound with captive portal logic
        setupCaptivePortalRoutes(webServer->getServer());
        LOG_INFO(TAG, "Captive portal enabled");
    } else {
        LOG_ERROR(TAG, "Web server failed to start in AP mode");
    }

    // Blue-white breathing LED for AP mode
    led->setBrightness(40);
    led->setEffect(LedEffect::BREATHING, LedColors::BLUE);

    buzzer->playConnect();
}

// =============================================================================
// Init helpers
// =============================================================================
static bool initNVS() {
    nvs = NVSManager::getInstance();
    if (!nvs->initialize()) {
        LOG_WARNING(TAG, "NVS init failed, attempting recovery...");
        nvs->resetToDefaults();
        if (!nvs->initialize()) {
            return false;
        }
        LOG_INFO(TAG, "NVS recovered after reset");
    }
    LOG_INFO(TAG, "NVS initialized");
    return true;
}

static bool initLittleFS() {
    if (!LittleFS.begin(true)) {
        LOG_ERROR(TAG, "LittleFS mount failed");
        return false;
    }
    LOG_INFO(TAG, "LittleFS mounted (total=%u, used=%u)",
             LittleFS.totalBytes(), LittleFS.usedBytes());
    return true;
}

static bool initDisplay() {
    SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);

    epd = new GxEPD2_BW<GxEPD2_290_T94, DISP_HEIGHT>(
        GxEPD2_290_T94(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
    );
    epd->init(0);  // 0 = no debug output to serial

    display = DisplayManager::getInstance();
    display->init(epd);
    LOG_INFO(TAG, "Display initialized (%dx%d)", DISP_WIDTH, DISP_HEIGHT);
    return true;
}

static bool connectWiFi() {
    wifi = WiFiManager::getInstance();
    wifi->begin();

    if (!wifi->autoConnect()) {
        LOG_ERROR(TAG, "WiFi autoConnect failed");
        return false;
    }

    LOG_INFO(TAG, "WiFi connected: SSID=%s IP=%s RSSI=%d",
             wifi->getSSID().c_str(), wifi->getIP().c_str(), wifi->getRSSI());
    buzzer->playConnect();
    return true;
}

static void initPostWiFi() {
    // ---- 14. Time sync (NTP) ----
    timeMgr = TimeManager::getInstance();
    timeMgr->begin();
    if (timeMgr->syncTime(15)) {
        LOG_INFO(TAG, "Time synced: %s %s",
                 timeMgr->getFormattedTime().c_str(),
                 timeMgr->getFormattedDate().c_str());
    } else {
        LOG_WARNING(TAG, "NTP sync failed, time may be invalid");
    }

    // ---- 15. mDNS init ----
    String hostname = config->getMdnsHostname();
    if (hostname.isEmpty()) {
        hostname = MDNS_HOSTNAME;
    }
    if (MDNS.begin(hostname.c_str())) {
        MDNS.addService("http", "tcp", WEBSERVER_PORT);
        LOG_INFO(TAG, "mDNS: %s.local", hostname.c_str());
    } else {
        LOG_ERROR(TAG, "mDNS init failed");
    }

    // ---- 16. Start web server ----
    webServer = InksPetWebServer::getInstance();
    if (webServer->begin()) {
        LOG_INFO(TAG, "Web server started on port %d", WEBSERVER_PORT);
    } else {
        LOG_ERROR(TAG, "Web server failed to start");
    }

    // ---- 17. Setup AgentStateManager callback ----
    agentMgr = AgentStateManager::getInstance();
    agentMgr->onStateChange(onAgentStateChange);
    LOG_INFO(TAG, "Agent state manager configured");

    // ---- 18. Setup PermissionManager response callback ----
    permMgr = PermissionManager::getInstance();
    permMgr->onResponse(onPermissionResponse);
    LOG_INFO(TAG, "Permission manager configured");

    // ---- Memory monitor ----
    memMonitor = MemoryMonitor::getInstance();

    // ---- 19. LED set to white dim (idle) ----
    led->setBrightnessLevel(config->getLedBrightness());
    led->setEffect(LedEffect::SOLID, LedColors::WHITE_DIM);

    // ---- 20. Show idle state on display ----
    currentDisplayMode = DisplayMode::AGENT_STATE;
    DisplayManager::AgentDisplayInfo idleInfo = {};
    idleInfo.stateName = "Idle";
    idleInfo.agentName = "";
    idleInfo.tool = "";
    idleInfo.file = "";
    idleInfo.pixelArt = getPixelArtForState("idle");
    idleInfo.elapsedMs = 0;
    idleInfo.activeSessions = 0;
    display->showAgentState(idleInfo);

    // ---- 21. Log ready ----
    LOG_INFO(TAG, "%s ready! IP=%s heap=%u",
             FIRMWARE_NAME, wifi->getIP().c_str(), ESP.getFreeHeap());
}
