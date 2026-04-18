#pragma once

// BuddyStateMapper — bridge the official 7-state buddy protocol to
// InksPet's 12-state AgentState enum (and emit visible side effects).
//
// Direction:                             Desktop snapshot  →  InksPet display
//   - counts (total/running/waiting)     → derive WORKING / THINKING / IDLE
//   - prompt != null                     → synthesize a PermissionManager request
//   - msg heuristic ("error", ...)       → ERROR
//   - one-shot triggers (heart/dizzy)    → drive a transient override state

#include <Arduino.h>
#include <functional>
#include "../AgentStateManager.h"

// 7 official buddy states (matches REFERENCE.md semantics).
enum class BuddyState : uint8_t {
    SLEEP = 0,
    IDLE,
    BUSY,
    ATTENTION,
    CELEBRATE,
    DIZZY,
    HEART
};

struct BuddySnapshot {
    uint8_t  total;
    uint8_t  running;
    uint8_t  waiting;
    bool     recentlyCompleted;
    uint32_t tokens;
    uint32_t tokensToday;
    // msg is UTF-8; one Chinese character takes 3 bytes. 64 bytes fits
    // ~20 汉字 or ~60 ASCII chars — enough for the upstream one-line
    // summaries we've seen ("done (success), 3 turns", "你帮我看看邮件").
    char     msg[64];
    bool     connected;           // derived from dataConnected()
    char     promptId[40];        // "" = no prompt
    char     promptTool[20];
    char     promptHint[64];
    char     lines[4][92];        // last 4 transcript lines (we cap at 4 for mem)
    uint8_t  nLines;
};

class BuddyStateMapper {
public:
    static BuddyStateMapper* getInstance();

    // Called by BuddyProtocol when a fresh snapshot lands.
    // Performs diff vs. last snapshot and:
    //   - enqueues/clears PermissionManager requests when prompt changes
    //   - triggers CELEBRATE when sessions drop to 0 right after a running period
    //   - pushes a synthetic event into AgentStateManager so downstream
    //     (display/LED/buzzer) reacts without knowing about the BLE path.
    void applySnapshot(const BuddySnapshot& s);

    // Called from buttons. Reports <5s approvals as HEART trigger.
    void noteApprovalSent(uint32_t tookSeconds);
    void noteDenialSent();

    // One-shot override — expires after durMs.
    void triggerOneShot(BuddyState s, uint32_t durMs);

    // Derive a BuddyState (7-state world) for the GIF renderer.
    BuddyState currentBuddyState() const;

    // Convert to the 12-state InksPet enum for the AgentStateManager.
    static AgentState buddyToAgent(BuddyState s);
    static const char* buddyStateName(BuddyState s);

    // Heartbeat liveness — sliding 30s window from last snapshot.
    bool isDataConnected() const;
    uint32_t lastSnapshotMs() const { return _lastSnapshotMs; }

    // Getters for display layer
    const BuddySnapshot& last() const { return _last; }

    // Subscribe to snapshot arrival. Fires inside applySnapshot() after
    // all domain-level side effects (permission queue, agent state update,
    // stats bookkeeping) have run, so the callback can safely read
    // AgentStateManager / PermissionManager state.
    using SnapshotCallback = std::function<void(const BuddySnapshot&)>;
    void onSnapshot(SnapshotCallback cb) { _snapshotCb = cb; }

private:
    BuddyStateMapper();
    static BuddyStateMapper* _instance;

    BuddySnapshot _last;
    bool          _hasLast;
    char          _lastPromptId[40];
    uint32_t      _lastSnapshotMs;
    uint32_t      _lastRunningAt;    // ms when running>0 last observed
    // One-shot override
    BuddyState    _oneShot;
    uint32_t      _oneShotUntil;
    bool          _oneShotActive;
    SnapshotCallback _snapshotCb;
};
