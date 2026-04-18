#include "BuddyStats.h"
#include <Preferences.h>
#include <string.h>
#include "../Logger.h"

static const char* TAG = "BuddyStats";
static const char* NS  = "buddy";

BuddyStats* BuddyStats::_instance = nullptr;

BuddyStats::BuddyStats()
    : _loaded(false), _dirty(false), _lastSaveMs(0),
      _appr(0), _deny(0), _velIdx(0), _velCnt(0), _lvl(0),
      _tok(0), _nap(0), _species(SPECIES_USE_GIF),
      _sSnd(true), _sLed(true), _napStartMs(0) {
    memset(_vel, 0, sizeof(_vel));
    _petName[0] = 0;
    _owner[0]   = 0;
    _charName[0] = 0;
}

BuddyStats* BuddyStats::getInstance() {
    if (!_instance) _instance = new BuddyStats();
    return _instance;
}

void BuddyStats::begin() {
    if (_loaded) return;
    Preferences p;
    if (!p.begin(NS, true)) {
        LOG_WARNING(TAG, "NVS namespace open failed; using defaults");
        _loaded = true;
        return;
    }
    _appr    = p.getUShort("appr", 0);
    _deny    = p.getUShort("deny", 0);
    _velIdx  = p.getUChar("vidx", 0);
    _velCnt  = p.getUChar("vcnt", 0);
    size_t vl = p.getBytes("vel", _vel, sizeof(_vel));
    if (vl != sizeof(_vel)) memset(_vel, 0, sizeof(_vel));
    _lvl     = p.getUChar("lvl", 0);
    _tok     = p.getUInt("tok", 0);
    _nap     = p.getUInt("nap", 0);
    String pn = p.getString("petname", "");
    String ow = p.getString("owner", "");
    String cn = p.getString("charname", "");
    strncpy(_petName, pn.c_str(), sizeof(_petName) - 1);
    strncpy(_owner,   ow.c_str(), sizeof(_owner) - 1);
    strncpy(_charName, cn.c_str(), sizeof(_charName) - 1);
    _species = p.getUChar("species", SPECIES_USE_GIF);
    _sSnd    = p.getBool("s_snd", true);
    _sLed    = p.getBool("s_led", true);
    p.end();

    _loaded = true;
    LOG_INFO(TAG, "Loaded: appr=%u deny=%u lvl=%u tok=%u species=%u",
             _appr, _deny, _lvl, _tok, _species);
}

void BuddyStats::save() {
    flushIfDirty();
}

void BuddyStats::markDirty() {
    _dirty = true;
    // Write-on-event with a 2s coalescing window keeps flash wear low.
    flushIfDirty();
}

void BuddyStats::flushIfDirty() {
    if (!_dirty) return;
    uint32_t now = millis();
    if (_lastSaveMs && (now - _lastSaveMs) < 2000) return;

    Preferences p;
    if (!p.begin(NS, false)) return;
    p.putUShort("appr", _appr);
    p.putUShort("deny", _deny);
    p.putUChar("vidx", _velIdx);
    p.putUChar("vcnt", _velCnt);
    p.putBytes("vel", _vel, sizeof(_vel));
    p.putUChar("lvl", _lvl);
    p.putUInt("tok", _tok);
    p.putUInt("nap", _nap);
    p.putString("petname", _petName);
    p.putString("owner", _owner);
    p.putString("charname", _charName);
    p.putUChar("species", _species);
    p.putBool("s_snd", _sSnd);
    p.putBool("s_led", _sLed);
    p.end();

    _lastSaveMs = now;
    _dirty = false;
}

void BuddyStats::onApproval(uint32_t tookSeconds) {
    _appr++;
    if (tookSeconds > 0xFFFF) tookSeconds = 0xFFFF;
    _vel[_velIdx] = (uint16_t)tookSeconds;
    _velIdx = (_velIdx + 1) % 8;
    if (_velCnt < 8) _velCnt++;
    markDirty();
}

void BuddyStats::onDenial() {
    _deny++;
    markDirty();
}

uint16_t BuddyStats::getMedianVelocity() const {
    if (_velCnt == 0) return 0;
    uint16_t tmp[8];
    memcpy(tmp, _vel, sizeof(uint16_t) * _velCnt);
    // simple insertion sort, n ≤ 8
    for (uint8_t i = 1; i < _velCnt; i++) {
        uint16_t v = tmp[i];
        int8_t j = i - 1;
        while (j >= 0 && tmp[j] > v) { tmp[j+1] = tmp[j]; j--; }
        tmp[j+1] = v;
    }
    return tmp[_velCnt / 2];
}

void BuddyStats::onBridgeTokens(uint32_t tokensCumulative) {
    // Upstream semantics: `tokens` is cumulative since bridge start.
    // We track total-seen across sessions for level computation.
    if (tokensCumulative > _tok) {
        _tok = tokensCumulative;
        uint8_t newLvl = (uint8_t)(_tok / TOKENS_PER_LEVEL);
        if (newLvl != _lvl) {
            _lvl = newLvl;
            LOG_INFO(TAG, "Level up: %u (tokens=%u)", _lvl, _tok);
        }
        markDirty();
    }
}

void BuddyStats::onNapStart() { _napStartMs = millis(); }
void BuddyStats::onNapEnd(uint32_t secs) {
    _nap += secs;
    _napStartMs = 0;
    markDirty();
}

void BuddyStats::setPetName(const char* name) {
    if (!name) return;
    strncpy(_petName, name, sizeof(_petName) - 1);
    _petName[sizeof(_petName) - 1] = 0;
    markDirty();
}

void BuddyStats::setOwner(const char* name) {
    if (!name) return;
    strncpy(_owner, name, sizeof(_owner) - 1);
    _owner[sizeof(_owner) - 1] = 0;
    markDirty();
}

void BuddyStats::setCharacterName(const char* name) {
    if (!name) { _charName[0] = 0; markDirty(); return; }
    strncpy(_charName, name, sizeof(_charName) - 1);
    _charName[sizeof(_charName) - 1] = 0;
    markDirty();
}

void BuddyStats::setSpecies(uint8_t idx)           { _species = idx; markDirty(); }
void BuddyStats::setSoundEnabled(bool v)           { _sSnd = v;      markDirty(); }
void BuddyStats::setLedOnAttention(bool v)         { _sLed = v;      markDirty(); }
