#include "EpaperGifRenderer.h"
#include <AnimatedGIF.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <string.h>
#include "BuddyStats.h"
#include "../DisplayManager.h"
#include "../Logger.h"
#include "../config.h"

static const char* TAG = "Gif";
EpaperGifRenderer* EpaperGifRenderer::_instance = nullptr;

// Single decoder instance — we play at most one GIF at a time.
static AnimatedGIF g_gif;
static File        g_gifFile;
static bool        g_gifFileValid = false;

// Forward declarations of C-style callbacks used by AnimatedGIF.
static void* _cbOpen(const char* filename, int32_t* pSize);
static void  _cbClose(void* handle);
static int32_t _cbRead(GIFFILE* pFile, uint8_t* pBuf, int32_t iLen);
static int32_t _cbSeek(GIFFILE* pFile, int32_t iPosition);
static void  _cbDraw(GIFDRAW* pDraw);

// 8×8 Bayer ordered dither matrix (values 0..63).
static const uint8_t kBayer8[8][8] = {
    { 0, 32,  8, 40,  2, 34, 10, 42},
    {48, 16, 56, 24, 50, 18, 58, 26},
    {12, 44,  4, 36, 14, 46,  6, 38},
    {60, 28, 52, 20, 62, 30, 54, 22},
    { 3, 35, 11, 43,  1, 33,  9, 41},
    {51, 19, 59, 27, 49, 17, 57, 25},
    {15, 47,  7, 39, 13, 45,  5, 37},
    {63, 31, 55, 23, 61, 29, 53, 21},
};

// Access to the renderer's framebuffer from the draw callback.
static EpaperGifRenderer* g_rendererSelf = nullptr;
// Expose framebuffer directly for the C callback via a file-scope pointer.
static uint8_t* g_fbPtr = nullptr;
static int16_t  g_offX = 0, g_offY = 0;

EpaperGifRenderer::EpaperGifRenderer()
    : _packLoaded(false), _idleCount(0), _idleIdx(0),
      _curState(BuddyState::IDLE), _gifOpen(false),
      _lastFrameMs(0), _nextFrameMinGapMs(1000), _forceFull(false) {
    _packName[0] = 0;
    _currentPath[0] = 0;
    memset(_statePath, 0, sizeof(_statePath));
    memset(_idleRotation, 0, sizeof(_idleRotation));
    memset(_fb, 0xFF, sizeof(_fb));  // start white
    g_rendererSelf = this;
    g_fbPtr = _fb;
}

EpaperGifRenderer* EpaperGifRenderer::getInstance() {
    if (!_instance) _instance = new EpaperGifRenderer();
    return _instance;
}

// ============================================================
// Manifest loading
// ============================================================

static const char* kStateKeys[7] = {
    "sleep", "idle", "busy", "attention", "celebrate", "dizzy", "heart"
};

bool EpaperGifRenderer::begin() {
    g_gif.begin(LITTLE_ENDIAN_PIXELS);
    return reload();
}

bool EpaperGifRenderer::reload() {
    _packLoaded = false;
    _idleCount = 0;
    _idleIdx = 0;
    memset(_statePath, 0, sizeof(_statePath));
    memset(_idleRotation, 0, sizeof(_idleRotation));

    const char* name = BuddyStats::getInstance()->getCharacterName();
    if (!name || !name[0]) {
        LOG_INFO(TAG, "No character pack installed — will use fallback pixel art");
        return false;
    }
    strncpy(_packName, name, sizeof(_packName) - 1);

    char mfPath[64];
    snprintf(mfPath, sizeof(mfPath), "/characters/%s/manifest.json", _packName);
    File mf = LittleFS.open(mfPath, "r");
    if (!mf) {
        LOG_WARNING(TAG, "No manifest at %s", mfPath);
        return false;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, mf);
    mf.close();
    if (err) {
        LOG_WARNING(TAG, "Manifest parse error: %s", err.c_str());
        return false;
    }

    JsonObject states = doc["states"].as<JsonObject>();
    if (states.isNull()) {
        LOG_WARNING(TAG, "No states in manifest");
        return false;
    }

    for (uint8_t i = 0; i < STATES; i++) {
        JsonVariant v = states[kStateKeys[i]];
        if (v.isNull()) continue;
        if (v.is<JsonArray>()) {
            JsonArray arr = v.as<JsonArray>();
            uint8_t n = 0;
            for (JsonVariant elt : arr) {
                const char* fn = elt | "";
                if (!fn[0]) continue;
                if (i == 1 /* idle */ && n < IDLE_SLOTS) {
                    snprintf(_idleRotation[n], sizeof(_idleRotation[n]),
                             "/characters/%s/%s", _packName, fn);
                    n++;
                }
            }
            _idleCount = n;
            if (n > 0) {
                strncpy(_statePath[i], _idleRotation[0], sizeof(_statePath[i]) - 1);
            }
        } else {
            const char* fn = v | "";
            if (fn[0]) {
                snprintf(_statePath[i], sizeof(_statePath[i]),
                         "/characters/%s/%s", _packName, fn);
            }
        }
    }

    _packLoaded = true;
    LOG_INFO(TAG, "Loaded pack: %s (idle rotation=%u)", _packName, _idleCount);
    _switchToState(_curState);
    return true;
}

const char* EpaperGifRenderer::_pickPathForState(BuddyState s) {
    uint8_t idx = (uint8_t)s;
    if (idx == 1 /* IDLE */ && _idleCount > 0) {
        return _idleRotation[_idleIdx];
    }
    return _statePath[idx];
}

// ============================================================
// Frame pump
// ============================================================

void EpaperGifRenderer::tick() {
    if (!_packLoaded) return;

    BuddyState next = BuddyStateMapper::getInstance()->currentBuddyState();
    if (next != _curState) {
        _switchToState(next);
    }

    uint32_t now = millis();
    if (now - _lastFrameMs < _nextFrameMinGapMs) return;

    if (!_gifOpen) {
        // State has no GIF — nothing to animate.
        return;
    }

    uint32_t gifDelayMs = 0;
    bool frameOk = _decodeOneFrame(gifDelayMs);

    if (frameOk) {
        _pushFbToPanel(_forceFull);
        _forceFull = false;
        _lastFrameMs = millis();
        // E-paper partial refresh floor; never poll faster than the panel
        // cooldown regardless of GIF timing.
        uint32_t floorMs = PARTIAL_REFRESH_COOLDOWN_MS;
        _nextFrameMinGapMs = gifDelayMs > floorMs ? gifDelayMs : floorMs;
    } else {
        // End of GIF — rewind; for idle rotation, advance to next clip.
        g_gif.reset();
        if (_curState == BuddyState::IDLE && _idleCount > 1) {
            _idleIdx = (_idleIdx + 1) % _idleCount;
            const char* p = _idleRotation[_idleIdx];
            _closeGif();
            _openGif(p);
        }
    }
}

void EpaperGifRenderer::_switchToState(BuddyState s) {
    _curState = s;
    const char* path = _pickPathForState(s);
    if (!path || !path[0]) {
        _closeGif();
        return;
    }
    if (strcmp(path, _currentPath) == 0 && _gifOpen) return;
    _closeGif();
    _openGif(path);
    _forceFull = true;   // new clip deserves a clean refresh
}

void EpaperGifRenderer::_openGif(const char* path) {
    strncpy(_currentPath, path, sizeof(_currentPath) - 1);
    _currentPath[sizeof(_currentPath) - 1] = 0;
    if (g_gif.open(_currentPath, _cbOpen, _cbClose, _cbRead, _cbSeek, _cbDraw)) {
        _gifOpen = true;
        LOG_INFO(TAG, "Playing %s (%dx%d)", _currentPath,
                 g_gif.getCanvasWidth(), g_gif.getCanvasHeight());
        // Center the GIF inside the framebuffer.
        int16_t w = g_gif.getCanvasWidth();
        int16_t h = g_gif.getCanvasHeight();
        g_offX = (FB_W - w) / 2;
        g_offY = (FB_H - h) / 2;
    } else {
        _gifOpen = false;
        LOG_WARNING(TAG, "gif.open failed for %s (err=%d)",
                    _currentPath, g_gif.getLastError());
    }
}

void EpaperGifRenderer::_closeGif() {
    if (_gifOpen) {
        g_gif.close();
        _gifOpen = false;
    }
    _currentPath[0] = 0;
}

bool EpaperGifRenderer::_decodeOneFrame(uint32_t& nextDelayMs) {
    _clearFb();
    int delayMs = 0;
    int rc = g_gif.playFrame(false, &delayMs);
    nextDelayMs = (uint32_t)(delayMs > 0 ? delayMs : 100);
    return rc > 0;
}

void EpaperGifRenderer::_clearFb() {
    memset(_fb, 0xFF, sizeof(_fb));   // white background
}

void EpaperGifRenderer::_pushFbToPanel(bool full) {
    DisplayManager* dm = DisplayManager::getInstance();
    auto* epd = dm->getDisplay();
    if (!epd) return;
    SemaphoreHandle_t mutex = dm->getMutex();
    if (mutex) xSemaphoreTake(mutex, portMAX_DELAY);

    // Drawing box spans the full screen.
    epd->setPartialWindow(0, 0, FB_W, FB_H);
    epd->firstPage();
    do {
        epd->fillScreen(GxEPD_WHITE);
        epd->drawBitmap(0, 0, _fb, FB_W, FB_H, GxEPD_BLACK);
    } while (epd->nextPage());

    if (mutex) xSemaphoreGive(mutex);
}

// ============================================================
// AnimatedGIF callbacks (file I/O against LittleFS)
// ============================================================

static void* _cbOpen(const char* filename, int32_t* pSize) {
    if (g_gifFileValid) { g_gifFile.close(); g_gifFileValid = false; }
    g_gifFile = LittleFS.open(filename, "r");
    if (!g_gifFile) return nullptr;
    g_gifFileValid = true;
    *pSize = (int32_t)g_gifFile.size();
    return (void*)&g_gifFile;
}

static void _cbClose(void* handle) {
    if (g_gifFileValid) { g_gifFile.close(); g_gifFileValid = false; }
}

static int32_t _cbRead(GIFFILE* pFile, uint8_t* pBuf, int32_t iLen) {
    File* f = (File*)pFile->fHandle;
    if (!f) return 0;
    int32_t n = f->read(pBuf, iLen);
    pFile->iPos = f->position();
    return n;
}

static int32_t _cbSeek(GIFFILE* pFile, int32_t iPosition) {
    File* f = (File*)pFile->fHandle;
    if (!f) return 0;
    f->seek(iPosition);
    pFile->iPos = iPosition;
    return iPosition;
}

// Per-scanline draw: RGB565 palette → luminance → Bayer8 threshold → 1-bit
// pixel written into the shared framebuffer.
static inline void _putPixel(int x, int y, bool black) {
    if (x < 0 || y < 0 || x >= 296 || y >= 128) return;
    size_t byteIdx = (size_t)y * (296 / 8) + (size_t)(x >> 3);
    uint8_t mask = 0x80 >> (x & 7);
    if (black) g_fbPtr[byteIdx] &= ~mask;
    else       g_fbPtr[byteIdx] |=  mask;
}

static void _cbDraw(GIFDRAW* d) {
    if (!g_fbPtr) return;
    uint16_t* palette = d->pPalette;
    uint8_t*  src     = d->pPixels;
    bool     hasT     = d->ucHasTransparency;
    uint8_t  tIdx     = d->ucTransparent;

    int y = g_offY + d->iY + d->y;
    if (y < 0 || y >= 128) return;

    int x0 = g_offX + d->iX;
    int w  = d->iWidth;

    const uint8_t* row = kBayer8[y & 7];

    for (int i = 0; i < w; i++) {
        uint8_t idx = src[i];
        if (hasT && idx == tIdx) continue;      // keep existing pixel
        uint16_t p = palette[idx];
        uint8_t r5 = (p >> 11) & 0x1F;
        uint8_t g6 = (p >>  5) & 0x3F;
        uint8_t b5 = p & 0x1F;
        // Approximate luminance in 0..63 range.
        uint8_t luma = (r5 + g6 + b5) / 2;
        int x = x0 + i;
        uint8_t t = row[x & 7];
        _putPixel(x, y, luma < t);
    }
}
