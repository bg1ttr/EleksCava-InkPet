#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include "config.h"

// Agent states ordered by display priority (higher = more important)
enum class AgentState : uint8_t {
    SLEEPING = 0,
    IDLE,
    COMPLETED,
    CARRYING,
    SWEEPING,
    JUGGLING,
    CONDUCTING,
    THINKING,
    WORKING,
    ERROR,
    PERMISSION
};

struct AgentSession {
    String agentName;
    String sessionId;
    AgentState state;
    String tool;
    String file;
    unsigned long lastUpdate;
    unsigned long sessionStart;   // When this session first appeared
    bool active;
    // Tool call statistics
    uint16_t toolCalls;           // Total tool invocations
    uint16_t reads;
    uint16_t writes;
    uint16_t edits;
    uint16_t bashes;
};

class AgentStateManager {
public:
    static AgentStateManager* getInstance();

    // Process incoming webhook event
    void processEvent(const JsonObject& event);

    // Get current highest-priority state
    AgentState getCurrentState() const;
    const AgentSession* getCurrentSession() const;

    // Permission management
    bool hasPendingPermission() const;
    const AgentSession* getPendingPermission() const;
    void respondToPermission(const String& sessionId, const String& action);

    // Session management
    int getActiveSessionCount() const;
    void cleanupStaleSessions();

    // State info
    static const char* stateToString(AgentState state);
    static const char* stateToDisplayName(AgentState state);

    // Callback
    using StateChangeCallback = std::function<void(AgentState newState, const AgentSession* session)>;
    void onStateChange(StateChangeCallback cb) { _callback = cb; }

private:
    AgentStateManager();
    static AgentStateManager* _instance;

    static const int MAX_SESSIONS = MAX_AGENT_SESSIONS;
    AgentSession _sessions[MAX_AGENT_SESSIONS];
    StateChangeCallback _callback;
    AgentState _lastReportedState;

    AgentSession* findOrCreateSession(const String& agent, const String& session);
    AgentState mapEventToState(const String& event, int activeSessions);
    void notifyStateChange();
};
