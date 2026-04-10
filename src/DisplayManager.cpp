#include "DisplayManager.h"
#include "Logger.h"
#include "version.h"
#include <esp_task_wdt.h>
#include <WiFi.h>

static const char* TAG = "Display";
DisplayManager* DisplayManager::_instance = nullptr;

DisplayManager::DisplayManager()
    : _display(nullptr), _mutex(nullptr), _partialCount(0), _initialized(false), _lastScreenType(0) {
    _mutex = xSemaphoreCreateMutex();
}

// Anti-ghosting: quick white clear (fast) or full black->white cycle (thorough)
void DisplayManager::deepClean() {
    if (!_display) return;
    // Only do the slow black->white cycle after many partial refreshes
    if (_partialCount > 15) {
        _display->setFullWindow();
        _display->firstPage();
        do { _display->fillScreen(GxEPD_BLACK); } while (_display->nextPage());
        delay(50);
    }
    // Always do a white clear
    _display->setFullWindow();
    _display->firstPage();
    do { _display->fillScreen(GxEPD_WHITE); } while (_display->nextPage());
    _partialCount = 0;
}

DisplayManager* DisplayManager::getInstance() {
    if (!_instance) _instance = new DisplayManager();
    return _instance;
}

void DisplayManager::init(GxEPD2_BW<GxEPD2_290_T94, DISP_HEIGHT>* d) {
    _display = d;
    _display->setRotation(1);  // Landscape mode (matching EleksCava reference)
    _u8g2.begin(*_display);
    _u8g2.setFontMode(0);
    _u8g2.setFontDirection(0);
    _u8g2.setForegroundColor(GxEPD_BLACK);
    _u8g2.setBackgroundColor(GxEPD_WHITE);
    _initialized = true;
    LOG_INFO(TAG, "Display initialized (%dx%d, rotation=1)", DISP_WIDTH, DISP_HEIGHT);
}

void DisplayManager::clearScreen() {
    if (!_initialized) return;
    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        _display->setFullWindow();
        _display->firstPage();
        do {
            _display->fillScreen(GxEPD_WHITE);
        } while (_display->nextPage());
        _partialCount = 0;
        xSemaphoreGive(_mutex);
    }
}

void DisplayManager::fullRefresh() {
    _partialCount = 0;
}

void DisplayManager::partialRefresh() {
    // Handled inline by display methods
}

void DisplayManager::incrementPartialCount() { _partialCount++; }

bool DisplayManager::needsFullRefresh() const {
    return _partialCount >= FULL_REFRESH_INTERVAL;
}

void DisplayManager::drawCenteredText(const char* text, int y, const uint8_t* font) {
    _u8g2.setFont(font);
    int16_t w = _u8g2.getUTF8Width(text);
    int16_t x = (DISP_WIDTH - w) / 2;
    _u8g2.setCursor(x, y);
    _u8g2.print(text);
}

void DisplayManager::drawTextAt(const char* text, int x, int y, const uint8_t* font) {
    _u8g2.setFont(font);
    _u8g2.setCursor(x, y);
    _u8g2.print(text);
}

// ---- Welcome Screen ----
void DisplayManager::showWelcome(const String& version, const String& mac) {
    if (!_initialized) return;
    esp_task_wdt_reset();

    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        _lastScreenType = 10;  // Welcome screen type
        _display->setFullWindow();
        _display->firstPage();
        do {
            _display->fillScreen(GxEPD_WHITE);

            // Title
            _u8g2.setForegroundColor(GxEPD_BLACK);
            drawCenteredText("InksPet", 45, u8g2_font_logisoso32_tr);

            // Subtitle
            drawCenteredText("AI Agent Desktop Companion", 68, u8g2_font_helvR08_tr);

            // Version + MAC
            char info[64];
            snprintf(info, sizeof(info), "v%s", version.c_str());
            drawCenteredText(info, 95, u8g2_font_helvR08_tr);

            if (mac.length() > 0) {
                drawCenteredText(mac.c_str(), 112, u8g2_font_helvR08_tr);
            }
        } while (_display->nextPage());

        _partialCount = 0;
        xSemaphoreGive(_mutex);
    }
}

// ---- AP Mode Screen ----
void DisplayManager::showAPMode(const String& ssid, const String& ip) {
    if (!_initialized) return;

    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        _lastScreenType = 11;
        _display->setFullWindow();
        _display->firstPage();
        do {
            _display->fillScreen(GxEPD_WHITE);

            // Header bar (white text on black background)
            _display->fillRect(0, 0, DISP_WIDTH, 24, GxEPD_BLACK);
            _u8g2.setForegroundColor(GxEPD_WHITE);
            _u8g2.setBackgroundColor(GxEPD_BLACK);
            drawCenteredText("WiFi Setup Mode", 18, u8g2_font_helvB10_tr);

            // Restore normal colors
            _u8g2.setForegroundColor(GxEPD_BLACK);
            _u8g2.setBackgroundColor(GxEPD_WHITE);

            // Instructions
            drawCenteredText("1. Connect to WiFi:", 46, u8g2_font_helvR08_tr);
            drawCenteredText(ssid.c_str(), 62, u8g2_font_helvB10_tr);

            drawCenteredText("2. Open browser:", 82, u8g2_font_helvR08_tr);

            char url[48];
            snprintf(url, sizeof(url), "http://%s", ip.c_str());
            drawCenteredText(url, 98, u8g2_font_helvB10_tr);

            drawCenteredText("3. Configure your WiFi network", 118, u8g2_font_helvR08_tr);
        } while (_display->nextPage());

        _partialCount = 0;
        xSemaphoreGive(_mutex);
    }
}

// ---- WiFi Connecting ----
void DisplayManager::showWiFiConnecting(const String& ssid) {
    if (!_initialized) return;

    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        _display->setFullWindow();
        _display->firstPage();
        do {
            _display->fillScreen(GxEPD_WHITE);
            drawCenteredText("Connecting to WiFi...", 50, u8g2_font_helvB10_tr);
            drawCenteredText(ssid.c_str(), 75, u8g2_font_helvR10_tr);
        } while (_display->nextPage());
        xSemaphoreGive(_mutex);
    }
}

// ---- WiFi Error ----
void DisplayManager::showWiFiError(const String& error) {
    if (!_initialized) return;

    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        _display->setFullWindow();
        _display->firstPage();
        do {
            _display->fillScreen(GxEPD_WHITE);
            drawCenteredText("WiFi Error", 40, u8g2_font_helvB10_tr);
            drawCenteredText(error.c_str(), 65, u8g2_font_helvR08_tr);
            drawCenteredText("Press A+C to enter setup mode", 100, u8g2_font_helvR08_tr);
        } while (_display->nextPage());
        xSemaphoreGive(_mutex);
    }
}

// ---- Generic Message ----
void DisplayManager::showMessage(const String& msg) {
    if (!_initialized) return;

    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        _display->setFullWindow();
        _display->firstPage();
        do {
            _display->fillScreen(GxEPD_WHITE);
            drawCenteredText(msg.c_str(), 70, u8g2_font_helvB10_tr);
        } while (_display->nextPage());
        xSemaphoreGive(_mutex);
    }
}

// ---- Error Display ----
void DisplayManager::showError(const String& error) {
    if (!_initialized) return;

    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        _display->setFullWindow();
        _display->firstPage();
        do {
            _display->fillScreen(GxEPD_WHITE);
            // Error icon area (white text on black background)
            _display->fillRect(0, 0, DISP_WIDTH, 28, GxEPD_BLACK);
            _u8g2.setForegroundColor(GxEPD_WHITE);
            _u8g2.setBackgroundColor(GxEPD_BLACK);
            drawCenteredText("! ERROR !", 20, u8g2_font_helvB10_tr);

            _u8g2.setForegroundColor(GxEPD_BLACK);
            _u8g2.setBackgroundColor(GxEPD_WHITE);
            drawCenteredText(error.c_str(), 65, u8g2_font_helvR08_tr);
        } while (_display->nextPage());
        xSemaphoreGive(_mutex);
    }
}

// ---- Agent State Display (core InksPet feature) ----
// Layout: black sidebar (62px) | right content | bottom black bar (16px)
// Inspired by Vibe Island's rich info display
void DisplayManager::showAgentState(const AgentDisplayInfo& info) {
    if (!_initialized) return;
    esp_task_wdt_reset();

    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        // Anti-ghosting: deep clean when switching from a different screen type
        if (_lastScreenType != 1) {
            deepClean();
            _lastScreenType = 1;
        }

        bool doFull = needsFullRefresh();

        if (doFull) {
            _display->setFullWindow();
        } else {
            _display->setPartialWindow(0, 0, DISP_WIDTH, DISP_HEIGHT);
        }

        _display->firstPage();
        do {
            _display->fillScreen(GxEPD_WHITE);

            // === Left black sidebar (0-62px) ===
            _display->fillRect(0, 0, 62, DISP_HEIGHT, GxEPD_BLACK);

            // Pixel art (white on black, vertically centered in sidebar)
            // Sidebar usable height = 112px (128 - 16 bottom bar)
            // Crab 48px + label ~12px + gap = ~64px, center at (112-64)/2 = 24
            if (info.pixelArt) {
                _display->drawXBitmap(7, 24, info.pixelArt, 48, 48, GxEPD_WHITE);
            }

            // State label in sidebar (white on black)
            _u8g2.setForegroundColor(GxEPD_WHITE);
            _u8g2.setBackgroundColor(GxEPD_BLACK);
            _u8g2.setFont(u8g2_font_helvR08_tr);
            int16_t labelW = _u8g2.getUTF8Width(info.stateName);
            int16_t labelX = (62 - labelW) / 2;
            if (labelX < 2) labelX = 2;
            _u8g2.setCursor(labelX, 86);
            _u8g2.print(info.stateName);

            // Session elapsed time in sidebar
            if (info.elapsedMs > 0) {
                char elapsed[12];
                unsigned long es = info.elapsedMs / 1000;
                if (es < 60) snprintf(elapsed, sizeof(elapsed), "%lus", es);
                else if (es < 3600) snprintf(elapsed, sizeof(elapsed), "%lum%02lus", es/60, es%60);
                else snprintf(elapsed, sizeof(elapsed), "%luh%02lum", es/3600, (es%3600)/60);
                int16_t elW = _u8g2.getUTF8Width(elapsed);
                _u8g2.setCursor((62 - elW) / 2, 98);
                _u8g2.print(elapsed);
            }

            // Restore normal colors
            _u8g2.setForegroundColor(GxEPD_BLACK);
            _u8g2.setBackgroundColor(GxEPD_WHITE);

            // === Right content area (68-292px) ===
            const int RX = 68;  // Right area start X

            // Row 1: State name (large bold) + active sessions badge
            drawTextAt(info.stateName, RX, 18, u8g2_font_helvB12_tr);
            if (info.activeSessions > 1) {
                char badge[8];
                snprintf(badge, sizeof(badge), "x%d", info.activeSessions);
                _u8g2.setFont(u8g2_font_helvR08_tr);
                int16_t stateW = _u8g2.getUTF8Width(info.stateName);
                // Switch font to measure badge with correct font
                _u8g2.setFont(u8g2_font_helvB12_tr);
                stateW = _u8g2.getUTF8Width(info.stateName);
                drawTextAt(badge, RX + stateW + 6, 18, u8g2_font_helvR08_tr);
            }

            // Row 2: Agent name with dot indicator
            if (info.agentName && strlen(info.agentName) > 0) {
                _display->fillRect(RX, 25, 3, 3, GxEPD_BLACK);
                drawTextAt(info.agentName, RX + 6, 30, u8g2_font_helvR08_tr);
            }

            // Dotted separator
            for (int i = RX; i < 292; i += 3) {
                _display->drawPixel(i, 36, GxEPD_BLACK);
            }

            // Row 3: Current tool + file (the main action line)
            if (info.tool && strlen(info.tool) > 0) {
                // Tool name in bold
                drawTextAt(info.tool, RX, 50, u8g2_font_helvB10_tr);
                // File path after tool
                if (info.file && strlen(info.file) > 0) {
                    _u8g2.setFont(u8g2_font_helvB10_tr);
                    int16_t toolW = _u8g2.getUTF8Width(info.tool);
                    String filePath(info.file);
                    if (filePath.length() > 28) {
                        filePath = "..." + filePath.substring(filePath.length() - 25);
                    }
                    drawTextAt(filePath.c_str(), RX + toolW + 5, 50, u8g2_font_helvR08_tr);
                }
            }

            // Row 4: Tool call statistics (R:5 W:3 E:2 B:1)
            if (info.toolCalls > 0) {
                char stats[48];
                snprintf(stats, sizeof(stats), "R:%u W:%u E:%u B:%u  [%u calls]",
                         info.reads, info.writes, info.edits, info.bashes, info.toolCalls);
                drawTextAt(stats, RX, 66, u8g2_font_helvR08_tr);
            }

            // === Bottom black info bar (static: IP + hostname) ===
            _display->fillRect(0, 112, DISP_WIDTH, 16, GxEPD_BLACK);
            _u8g2.setForegroundColor(GxEPD_WHITE);
            _u8g2.setBackgroundColor(GxEPD_BLACK);

            // Left: device IP
            String ip = WiFi.localIP().toString();
            if (ip == "0.0.0.0") ip = "inkspet.local";
            drawTextAt(ip.c_str(), 4, 124, u8g2_font_helvR08_tr);

            // Right: hostname
            _u8g2.setFont(u8g2_font_helvR08_tr);
            const char* host = "inkspet.local";
            if (ip != "inkspet.local") {
                int16_t hostW = _u8g2.getUTF8Width(host);
                _u8g2.setCursor(DISP_WIDTH - hostW - 4, 124);
                _u8g2.print(host);
            }

            // Restore colors
            _u8g2.setForegroundColor(GxEPD_BLACK);
            _u8g2.setBackgroundColor(GxEPD_WHITE);

        } while (_display->nextPage());

        if (doFull) {
            _partialCount = 0;
        } else {
            _partialCount++;
        }

        xSemaphoreGive(_mutex);
    }
    esp_task_wdt_reset();
}

// ---- Permission Request Screen ----
// Layout: full-width header | tool highlight box | three button columns
void DisplayManager::showPermissionRequest(const char* agentName, const char* tool,
                                            const char* file) {
    if (!_initialized) return;

    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        _lastScreenType = 2;  // Full refresh screen, no deepClean needed
        _display->setFullWindow();
        _display->firstPage();
        do {
            _display->fillScreen(GxEPD_WHITE);

            // === Top header bar (black, 18px) ===
            _display->fillRect(0, 0, DISP_WIDTH, 18, GxEPD_BLACK);
            _u8g2.setForegroundColor(GxEPD_WHITE);
            _u8g2.setBackgroundColor(GxEPD_BLACK);
            drawTextAt("! PERMISSION REQUEST", 4, 13, u8g2_font_helvB08_tr);
            _u8g2.setForegroundColor(GxEPD_BLACK);
            _u8g2.setBackgroundColor(GxEPD_WHITE);

            // === Content area ===
            char line1[64];
            snprintf(line1, sizeof(line1), "%s wants to:", agentName);
            drawTextAt(line1, 8, 36, u8g2_font_helvR08_tr);

            // Tool + file in highlighted black box
            _display->fillRect(8, 42, 280, 20, GxEPD_BLACK);
            _u8g2.setForegroundColor(GxEPD_WHITE);
            _u8g2.setBackgroundColor(GxEPD_BLACK);
            char toolLine[80];
            if (file && strlen(file) > 0) {
                String filePath(file);
                if (filePath.length() > 30) {
                    filePath = "..." + filePath.substring(filePath.length() - 27);
                }
                snprintf(toolLine, sizeof(toolLine), "%s  %s", tool, filePath.c_str());
            } else {
                snprintf(toolLine, sizeof(toolLine), "%s", tool);
            }
            drawTextAt(toolLine, 14, 56, u8g2_font_helvB08_tr);
            _u8g2.setForegroundColor(GxEPD_BLACK);
            _u8g2.setBackgroundColor(GxEPD_WHITE);

            // === Separator line ===
            _display->drawFastHLine(0, 90, DISP_WIDTH, GxEPD_BLACK);

            // === Three button columns ===
            const int btnW = 98;
            const int btnY = 96;
            const int btnH = 28;

            // [A] Allow - solid black (primary action)
            _display->fillRect(2, btnY, btnW - 4, btnH, GxEPD_BLACK);
            _u8g2.setForegroundColor(GxEPD_WHITE);
            _u8g2.setBackgroundColor(GxEPD_BLACK);
            drawTextAt("[A] ALLOW", 14, btnY + 18, u8g2_font_helvB08_tr);

            // [B] Always - outlined
            _u8g2.setForegroundColor(GxEPD_BLACK);
            _u8g2.setBackgroundColor(GxEPD_WHITE);
            _display->drawRect(btnW + 2, btnY, btnW - 4, btnH, GxEPD_BLACK);
            drawTextAt("[B] ALWAYS", btnW + 10, btnY + 18, u8g2_font_helvB08_tr);

            // [C] Deny - outlined
            _display->drawRect(btnW * 2 + 2, btnY, btnW - 4, btnH, GxEPD_BLACK);
            drawTextAt("[C] DENY", btnW * 2 + 14, btnY + 18, u8g2_font_helvB08_tr);

        } while (_display->nextPage());

        _partialCount = 0;
        xSemaphoreGive(_mutex);
    }
}

// ---- Device Info Screen ----
void DisplayManager::showDeviceInfo(const String& ip, const String& mac,
                                     int rssi, float battVolt, int battPct,
                                     size_t freeHeap) {
    if (!_initialized) return;

    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        _lastScreenType = 3;
        _display->setFullWindow();
        _display->firstPage();
        do {
            _display->fillScreen(GxEPD_WHITE);

            // Header (white text on black background)
            _display->fillRect(0, 0, DISP_WIDTH, 22, GxEPD_BLACK);
            _u8g2.setForegroundColor(GxEPD_WHITE);
            _u8g2.setBackgroundColor(GxEPD_BLACK);
            drawCenteredText("InksPet Device Info", 16, u8g2_font_helvB08_tr);

            _u8g2.setForegroundColor(GxEPD_BLACK);
            _u8g2.setBackgroundColor(GxEPD_WHITE);

            char line[64];
            int y = 40;
            const int lineH = 16;

            snprintf(line, sizeof(line), "IP: %s", ip.c_str());
            drawTextAt(line, 8, y, u8g2_font_helvR08_tr);
            y += lineH;

            snprintf(line, sizeof(line), "MAC: %s", mac.c_str());
            drawTextAt(line, 8, y, u8g2_font_helvR08_tr);
            y += lineH;

            snprintf(line, sizeof(line), "WiFi: %ddBm", rssi);
            drawTextAt(line, 8, y, u8g2_font_helvR08_tr);

            snprintf(line, sizeof(line), "Batt: %.1fV (%d%%)", battVolt, battPct);
            drawTextAt(line, 150, y, u8g2_font_helvR08_tr);
            y += lineH;

            snprintf(line, sizeof(line), "Heap: %uB free", freeHeap);
            drawTextAt(line, 8, y, u8g2_font_helvR08_tr);

            snprintf(line, sizeof(line), "FW: v%s", VERSION);
            drawTextAt(line, 150, y, u8g2_font_helvR08_tr);
            y += lineH;

            unsigned long upSec = millis() / 1000;
            snprintf(line, sizeof(line), "Uptime: %luh%02lum", upSec / 3600, (upSec % 3600) / 60);
            drawTextAt(line, 8, y, u8g2_font_helvR08_tr);

        } while (_display->nextPage());

        _partialCount = 0;
        xSemaphoreGive(_mutex);
    }
}

// ---- Time Mode ----
void DisplayManager::showTimeMode(const String& time, const String& date) {
    if (!_initialized) return;

    if (xSemaphoreTake(_mutex, portMAX_DELAY)) {
        _lastScreenType = 4;
        bool doFull = needsFullRefresh();

        if (doFull) {
            _display->setFullWindow();
        } else {
            _display->setPartialWindow(0, 0, DISP_WIDTH, DISP_HEIGHT);
        }

        _display->firstPage();
        do {
            _display->fillScreen(GxEPD_WHITE);

            // Large time
            drawCenteredText(time.c_str(), 72, u8g2_font_logisoso32_tr);

            // Date below
            drawCenteredText(date.c_str(), 105, u8g2_font_helvR10_tr);

        } while (_display->nextPage());

        if (doFull) _partialCount = 0;
        else _partialCount++;

        xSemaphoreGive(_mutex);
    }
}
