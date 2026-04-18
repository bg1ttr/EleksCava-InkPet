#pragma once

// EpaperGifRenderer — plays the official 7-state GIF character packs on a
// 296×128 1-bit e-paper panel. Runs the AnimatedGIF decoder but redirects
// the per-scanline draw callback into an in-RAM 1bpp framebuffer, with
// Bayer 8×8 ordered dithering so colour GIFs degrade gracefully.
//
// Frame rate is capped by e-paper physics (~300 ms partial refresh + 800 ms
// cooldown). We pick one representative frame per "tick window" and push
// that to the panel; the GIF's own frame durations drive which frame wins.
//
// Character pack format (protocol parity): /characters/<name>/manifest.json
// declaring 7 states (sleep, idle, busy, attention, celebrate, dizzy, heart)
// mapped to .gif filenames. idle may be an array — we rotate per loop-end.

#include <Arduino.h>
#include "BuddyStateMapper.h"

class EpaperGifRenderer {
public:
    static EpaperGifRenderer* getInstance();

    // Load manifest.json from /characters/<name>/ (name from BuddyStats).
    // Returns true if a valid pack was found and at least one GIF opened.
    bool begin();

    // Reload manifest (call after a folder push completes).
    bool reload();

    // Drive animation. Call from loop().
    // Will:
    //   1. pick current BuddyState from BuddyStateMapper
    //   2. if state changed, switch GIF (and advance idle-array rotation)
    //   3. if frame-pacing window elapsed, decode one frame → framebuffer
    //      → partial refresh
    void tick();

    // Force a full refresh on next tick (called from the 10-minute ghosting
    // timer already present in main.cpp).
    void requestFullRefresh() { _forceFull = true; }

    bool hasCharacterPack() const { return _packLoaded; }
    const char* getPackName() const { return _packName; }

private:
    EpaperGifRenderer();
    static EpaperGifRenderer* _instance;

    bool   _packLoaded;
    char   _packName[32];

    // Per-state GIF paths. For idle we store up to 6 files and rotate.
    static const uint8_t STATES = 7;
    static const uint8_t IDLE_SLOTS = 6;
    char   _statePath[STATES][48];       // primary path per state
    char   _idleRotation[IDLE_SLOTS][48];
    uint8_t _idleCount;
    uint8_t _idleIdx;

    BuddyState _curState;
    bool       _gifOpen;
    uint32_t   _lastFrameMs;
    uint32_t   _nextFrameMinGapMs;   // honor GIF delay ≥ 300ms e-paper floor
    bool       _forceFull;
    char       _currentPath[48];

    // 1-bit framebuffer (MSB = leftmost pixel). Drawn via DisplayManager.
    static const uint16_t FB_W = 296;
    static const uint16_t FB_H = 128;
    uint8_t _fb[(FB_W * FB_H) / 8];

    void _clearFb();
    void _pushFbToPanel(bool full);
    void _switchToState(BuddyState s);
    void _openGif(const char* path);
    void _closeGif();
    bool _decodeOneFrame(uint32_t& nextDelayMs);
    const char* _pickPathForState(BuddyState s);
};
