#include "BuddyStateMapper.h"
#include <ArduinoJson.h>
#include <string.h>
#include "../Logger.h"
#include "../PermissionManager.h"

static const char* TAG = "Mapper";
BuddyStateMapper* BuddyStateMapper::_instance = nullptr;

BuddyStateMapper::BuddyStateMapper()
    : _hasLast(false), _lastSnapshotMs(0), _lastRunningAt(0),
      _oneShot(BuddyState::IDLE), _oneShotUntil(0), _oneShotActive(false) {
    memset(&_last, 0, sizeof(_last));
    _lastPromptId[0] = 0;
}

BuddyStateMapper* BuddyStateMapper::getInstance() {
    if (!_instance) _instance = new BuddyStateMapper();
    return _instance;
}

static bool strEq(const char* a, const char* b) {
    return strncmp(a, b, 40) == 0;
}

void BuddyStateMapper::applySnapshot(const BuddySnapshot& s) {
    _last = s;
    _hasLast = true;
    _lastSnapshotMs = millis();

    if (s.running > 0) _lastRunningAt = _lastSnapshotMs;

    // Prompt diff → feed PermissionManager
    PermissionManager* pm = PermissionManager::getInstance();
    if (!strEq(s.promptId, _lastPromptId)) {
        strncpy(_lastPromptId, s.promptId, sizeof(_lastPromptId) - 1);
        _lastPromptId[sizeof(_lastPromptId) - 1] = 0;

        if (s.promptId[0]) {
            // New prompt — enqueue. Session id = promptId so response
            // can be routed back through the same correlation field.
            LOG_INFO(TAG, "Prompt via BLE: id=%s tool=%s hint=%s",
                     s.promptId, s.promptTool, s.promptHint);
            pm->queueRequest(
                String(s.promptId),
                String("Claude"),
                String(s.promptTool[0] ? s.promptTool : "?"),
                String(s.promptHint),
                PermissionManager::Source::BLE);
        }
        // When promptId becomes empty, the desktop has resolved it from
        // another surface; let the existing queue drain on timeout or button.
    }

    // Synthesize an AgentStateManager event so display/LED pipelines fire.
    // Mapping heuristics:
    //   - prompt != null       → PermissionRequest
    //   - msg contains "error" → PostToolUseFailure
    //   - waiting>0            → PermissionRequest (fallback)
    //   - running>0            → PreToolUse (becomes WORKING)
    //   - running==0, total>0  → UserPromptSubmit (THINKING)
    //   - total==0             → Stop (COMPLETED) → settles to IDLE/SLEEP
    StaticJsonDocument<256> doc;
    JsonObject ev = doc.to<JsonObject>();
    ev["agent"]   = "Claude";
    ev["session"] = s.promptId[0] ? s.promptId : "desktop";
    ev["tool"]    = s.promptTool;
    ev["file"]    = s.promptHint;

    bool msgError = false;
    for (const char* p = s.msg; *p; ++p) {
        if ((p[0] == 'e' || p[0] == 'E') && (p[1] == 'r' || p[1] == 'R') &&
            (p[2] == 'r' || p[2] == 'R')) { msgError = true; break; }
    }

    // Only push a synthetic event when there's actual activity on the
    // desktop. `total > 0` alone just means the user has session windows
    // open — it doesn't mean Claude is generating anything. Fabricating
    // UserPromptSubmit in that case puts the device into THINKING with
    // a blue breathing LED while the HUD truthfully says "Idle".
    const char* eventType = nullptr;
    if (s.promptId[0])                 eventType = "PermissionRequest";
    else if (msgError)                 eventType = "PostToolUseFailure";
    else if (s.waiting > 0)            eventType = "PermissionRequest";
    else if (s.running > 0)            eventType = "PreToolUse";
    else if (s.recentlyCompleted)      eventType = "Stop";
    // else: desktop is open but quiet. Stale 'desktop' sessions age out
    // via AgentStateManager::cleanupStaleSessions (30s timeout), which
    // settles display/LED back to IDLE/SLEEPING on their own.

    if (eventType) {
        ev["event"] = eventType;
        AgentStateManager::getInstance()->processEvent(ev);
    }

    if (_snapshotCb) _snapshotCb(_last);
}

void BuddyStateMapper::noteApprovalSent(uint32_t tookSeconds) {
    if (tookSeconds < 5) {
        triggerOneShot(BuddyState::HEART, 2000);
    }
}

void BuddyStateMapper::noteDenialSent() {
    // No visual override for denials; the buzzer/LED feedback handles it.
}

void BuddyStateMapper::triggerOneShot(BuddyState s, uint32_t durMs) {
    _oneShot = s;
    _oneShotUntil = millis() + durMs;
    _oneShotActive = true;
}

BuddyState BuddyStateMapper::currentBuddyState() const {
    uint32_t now = millis();
    if (_oneShotActive && (int32_t)(now - _oneShotUntil) < 0) {
        return _oneShot;
    }
    // Official derive():
    //   !connected               → IDLE (we show SLEEP after 30s of no snapshot)
    //   waiting > 0              → ATTENTION
    //   recentlyCompleted        → CELEBRATE
    //   running >= 3             → BUSY
    //   else                     → IDLE
    if (!isDataConnected()) return BuddyState::SLEEP;
    if (_last.waiting > 0)   return BuddyState::ATTENTION;
    if (_last.recentlyCompleted) return BuddyState::CELEBRATE;
    if (_last.running >= 3)  return BuddyState::BUSY;
    if (_last.running > 0)   return BuddyState::BUSY;
    return BuddyState::IDLE;
}

bool BuddyStateMapper::isDataConnected() const {
    if (!_hasLast) return false;
    uint32_t age = millis() - _lastSnapshotMs;
    return age <= 30000;
}

AgentState BuddyStateMapper::buddyToAgent(BuddyState s) {
    switch (s) {
        case BuddyState::SLEEP:     return AgentState::SLEEPING;
        case BuddyState::IDLE:      return AgentState::IDLE;
        case BuddyState::BUSY:      return AgentState::WORKING;
        case BuddyState::ATTENTION: return AgentState::PERMISSION;
        case BuddyState::CELEBRATE: return AgentState::COMPLETED;
        case BuddyState::DIZZY:     return AgentState::ERROR;
        case BuddyState::HEART:     return AgentState::COMPLETED;
    }
    return AgentState::IDLE;
}

const char* BuddyStateMapper::buddyStateName(BuddyState s) {
    switch (s) {
        case BuddyState::SLEEP:     return "sleep";
        case BuddyState::IDLE:      return "idle";
        case BuddyState::BUSY:      return "busy";
        case BuddyState::ATTENTION: return "attention";
        case BuddyState::CELEBRATE: return "celebrate";
        case BuddyState::DIZZY:     return "dizzy";
        case BuddyState::HEART:     return "heart";
    }
    return "idle";
}
