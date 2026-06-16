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
    ASK,
    PERMISSION
};

struct AgentSession {
    static constexpr uint8_t MAX_TASK_ITEMS = 3;

    String agentName;
    String sessionId;
    AgentState state;
    String tool;
    String file;
    String taskTitle;
    String taskSummary;
    String promptSnippet;
    String repo;
    String branch;
    String currentAction;
    String language;
    String usageLine1;
    String usageLine2;
    String errorMessage;
    String recentCompletionTitle;
    String recentCompletionAgo;
    String permissionCommand;
    unsigned long lastUpdate;
    unsigned long sessionStart;   // When this session first appeared
    bool active;
    int16_t priority;
    // Tool call statistics
    uint16_t toolCalls;           // Total tool invocations
    uint16_t reads;
    uint16_t writes;
    uint16_t edits;
    uint16_t bashes;
    // Task progress (from TodoWrite tool)
    uint16_t tasksDone;
    uint16_t tasksRunning;
    uint16_t tasksPending;
    bool hasTasks;                // Whether any TodoWrite data has been received
    String taskItems[MAX_TASK_ITEMS];
    uint8_t taskItemCount;
    bool hasReliableProgress;
    uint16_t progressDone;
    uint16_t progressTotal;
    bool hasUsage;
    String questionOptions[MAX_TASK_ITEMS];
    uint8_t questionOptionCount;
};

class AgentStateManager {
public:
    static AgentStateManager* getInstance();

    // Process incoming webhook event
    void processEvent(const JsonObject& event);

    // Update task progress from TodoWrite hook
    void updateTasks(const String& sessionId, uint16_t done, uint16_t running, uint16_t pending,
                     const String* items = nullptr, uint8_t itemCount = 0);

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
    static AgentState stateFromString(const String& state, AgentState fallback);

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
    void resetSessionFields(AgentSession* session, const String& agent, const String& sessionId);
};
