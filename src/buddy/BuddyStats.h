#pragma once

// BuddyStats — NVS-persisted counters and settings for Claude buddy protocol.
// Mirrors the upstream `stats.h` key layout so future behaviors/tools can interop.
// Namespace: "buddy" (distinct from "wifi" and "inkspet").

#include <Arduino.h>
#include <stdint.h>

class BuddyStats {
public:
    static BuddyStats* getInstance();

    void begin();     // Load all values from NVS
    void save();      // Persist if dirty

    // Approvals / denials
    void onApproval(uint32_t tookSeconds);
    void onDenial();

    uint16_t getApprovals() const { return _appr; }
    uint16_t getDenials()   const { return _deny; }

    // Velocity ring — 8 slots of seconds-to-respond; used for "vel" stat.
    uint16_t getMedianVelocity() const;

    // Token totals (cumulative since first pairing)
    void onBridgeTokens(uint32_t tokensCumulative);
    uint32_t getTokens() const { return _tok; }
    uint8_t  getLevel()  const { return _lvl; }
    static const uint32_t TOKENS_PER_LEVEL = 50000;

    // Nap / energy
    void onNapStart();
    void onNapEnd(uint32_t secs);
    uint32_t getNapSeconds() const { return _nap; }

    // Names
    void setPetName(const char* name);
    void setOwner(const char* name);
    const char* getPetName() const { return _petName; }
    const char* getOwner()   const { return _owner; }

    // Species / character
    void setSpecies(uint8_t idx);
    uint8_t getSpecies() const { return _species; }
    static const uint8_t SPECIES_USE_GIF = 0xFF;

    // Settings (stored toggles). We keep these for protocol parity — the
    // actual enforcement lives in ConfigManager for LED/sound.
    void setSoundEnabled(bool v);
    void setLedOnAttention(bool v);
    bool getSoundEnabled() const    { return _sSnd; }
    bool getLedOnAttention() const  { return _sLed; }

    // Character-pack name (last received via folder push).
    void setCharacterName(const char* name);
    const char* getCharacterName() const { return _charName; }

private:
    BuddyStats();
    static BuddyStats* _instance;

    void markDirty();
    void flushIfDirty();

    bool     _loaded;
    bool     _dirty;
    uint32_t _lastSaveMs;

    // Persisted fields
    uint16_t _appr;
    uint16_t _deny;
    uint16_t _vel[8];
    uint8_t  _velIdx;
    uint8_t  _velCnt;
    uint8_t  _lvl;
    uint32_t _tok;
    uint32_t _nap;
    char     _petName[24];
    char     _owner[32];
    char     _charName[32];
    uint8_t  _species;
    bool     _sSnd;
    bool     _sLed;

    // Runtime
    uint32_t _napStartMs;
};
