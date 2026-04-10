#include "AgentStateManager.h"
#include "Logger.h"
#include "config.h"

static const char* TAG = "AgentState";
AgentStateManager* AgentStateManager::_instance = nullptr;

AgentStateManager::AgentStateManager()
    : _callback(nullptr), _lastReportedState(AgentState::SLEEPING) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        _sessions[i].active = false;
    }
}

AgentStateManager* AgentStateManager::getInstance() {
    if (!_instance) _instance = new AgentStateManager();
    return _instance;
}

void AgentStateManager::processEvent(const JsonObject& event) {
    String agentName = event["agent"] | "unknown";
    String sessionId = event["session"] | agentName;
    String eventType = event["event"] | "";
    String tool = event["tool"] | "";
    String file = event["file"] | "";

    LOG_INFO(TAG, "Event: agent=%s session=%s event=%s tool=%s",
             agentName.c_str(), sessionId.c_str(), eventType.c_str(), tool.c_str());

    AgentSession* session = findOrCreateSession(agentName, sessionId);
    if (!session) {
        LOG_ERROR(TAG, "No available session slot");
        return;
    }

    session->tool = tool;
    session->file = file;
    session->lastUpdate = millis();
    session->active = true;

    // Track tool call statistics
    if (eventType == "PreToolUse") {
        session->toolCalls++;
        if (tool == "Read") session->reads++;
        else if (tool == "Write") session->writes++;
        else if (tool == "Edit" || tool == "MultiEdit") session->edits++;
        else if (tool == "Bash") session->bashes++;
    }

    // Map event to state
    int activeSessions = getActiveSessionCount();
    AgentState newState = mapEventToState(eventType, activeSessions);
    session->state = newState;

    // Handle Stop event - mark as completed
    if (eventType == "Stop") {
        session->state = AgentState::COMPLETED;
    }

    notifyStateChange();
}

AgentState AgentStateManager::mapEventToState(const String& event, int activeSessions) {
    if (event == "PermissionRequest") return AgentState::PERMISSION;
    if (event == "PostToolUseFailure") return AgentState::ERROR;
    if (event == "PreToolUse" || event == "PostToolUse") return AgentState::WORKING;
    if (event == "UserPromptSubmit") return AgentState::THINKING;
    if (event == "SubagentStart") {
        return (activeSessions >= 3) ? AgentState::CONDUCTING : AgentState::JUGGLING;
    }
    if (event == "PreCompact") return AgentState::SWEEPING;
    if (event == "WorktreeCreate") return AgentState::CARRYING;
    if (event == "Stop" || event == "PostCompact") return AgentState::COMPLETED;

    return AgentState::IDLE;
}

AgentState AgentStateManager::getCurrentState() const {
    AgentState highest = AgentState::SLEEPING;
    bool anyActive = false;

    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (_sessions[i].active) {
            anyActive = true;
            if (static_cast<uint8_t>(_sessions[i].state) > static_cast<uint8_t>(highest)) {
                highest = _sessions[i].state;
            }
        }
    }

    if (!anyActive) {
        return AgentState::SLEEPING;
    }

    return highest;
}

const AgentSession* AgentStateManager::getCurrentSession() const {
    AgentState highest = getCurrentState();

    // Return the session with the highest priority state (most recent if tied)
    const AgentSession* best = nullptr;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (_sessions[i].active && _sessions[i].state == highest) {
            if (!best || _sessions[i].lastUpdate > best->lastUpdate) {
                best = &_sessions[i];
            }
        }
    }
    return best;
}

bool AgentStateManager::hasPendingPermission() const {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (_sessions[i].active && _sessions[i].state == AgentState::PERMISSION) {
            return true;
        }
    }
    return false;
}

const AgentSession* AgentStateManager::getPendingPermission() const {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (_sessions[i].active && _sessions[i].state == AgentState::PERMISSION) {
            return &_sessions[i];
        }
    }
    return nullptr;
}

void AgentStateManager::respondToPermission(const String& sessionId, const String& action) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (_sessions[i].active && _sessions[i].sessionId == sessionId &&
            _sessions[i].state == AgentState::PERMISSION) {
            _sessions[i].state = AgentState::WORKING;
            LOG_INFO(TAG, "Permission %s for session %s", action.c_str(), sessionId.c_str());
            notifyStateChange();
            return;
        }
    }
    LOG_WARNING(TAG, "No pending permission for session %s", sessionId.c_str());
}

int AgentStateManager::getActiveSessionCount() const {
    int count = 0;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (_sessions[i].active) count++;
    }
    return count;
}

void AgentStateManager::cleanupStaleSessions() {
    unsigned long now = millis();
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (_sessions[i].active && (now - _sessions[i].lastUpdate > SLEEP_TIMEOUT_MS)) {
            LOG_INFO(TAG, "Session %s timed out", _sessions[i].sessionId.c_str());
            _sessions[i].active = false;
        }
    }
}

AgentSession* AgentStateManager::findOrCreateSession(const String& agent, const String& session) {
    // Find existing session by ID
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (_sessions[i].active && _sessions[i].sessionId == session) {
            return &_sessions[i];
        }
    }

    // Find empty slot
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!_sessions[i].active) {
            _sessions[i].agentName = agent;
            _sessions[i].sessionId = session;
            _sessions[i].state = AgentState::IDLE;
            _sessions[i].active = true;
            _sessions[i].lastUpdate = millis();
            _sessions[i].sessionStart = millis();
            _sessions[i].toolCalls = 0;
            _sessions[i].reads = 0;
            _sessions[i].writes = 0;
            _sessions[i].edits = 0;
            _sessions[i].bashes = 0;
            LOG_INFO(TAG, "New session: %s (%s)", session.c_str(), agent.c_str());
            return &_sessions[i];
        }
    }

    // Evict oldest session
    int oldest = 0;
    for (int i = 1; i < MAX_SESSIONS; i++) {
        if (_sessions[i].lastUpdate < _sessions[oldest].lastUpdate) {
            oldest = i;
        }
    }
    LOG_WARNING(TAG, "Evicting session %s for %s",
                _sessions[oldest].sessionId.c_str(), session.c_str());
    _sessions[oldest].agentName = agent;
    _sessions[oldest].sessionId = session;
    _sessions[oldest].state = AgentState::IDLE;
    _sessions[oldest].active = true;
    _sessions[oldest].lastUpdate = millis();
    return &_sessions[oldest];
}

void AgentStateManager::notifyStateChange() {
    AgentState current = getCurrentState();
    if (current != _lastReportedState) {
        _lastReportedState = current;
        if (_callback) {
            _callback(current, getCurrentSession());
        }
    }
}

const char* AgentStateManager::stateToString(AgentState state) {
    switch (state) {
        case AgentState::SLEEPING:   return "sleeping";
        case AgentState::IDLE:       return "idle";
        case AgentState::THINKING:   return "thinking";
        case AgentState::WORKING:    return "working";
        case AgentState::ERROR:      return "error";
        case AgentState::COMPLETED:  return "completed";
        case AgentState::PERMISSION: return "permission";
        case AgentState::JUGGLING:   return "juggling";
        case AgentState::CONDUCTING: return "conducting";
        case AgentState::SWEEPING:   return "sweeping";
        case AgentState::CARRYING:   return "carrying";
        default:                     return "unknown";
    }
}

const char* AgentStateManager::stateToDisplayName(AgentState state) {
    switch (state) {
        case AgentState::SLEEPING:   return "Sleeping";
        case AgentState::IDLE:       return "Idle";
        case AgentState::THINKING:   return "Thinking...";
        case AgentState::WORKING:    return "Working";
        case AgentState::ERROR:      return "Error!";
        case AgentState::COMPLETED:  return "Done!";
        case AgentState::PERMISSION: return "Permission?";
        case AgentState::JUGGLING:   return "Juggling";
        case AgentState::CONDUCTING: return "Conducting";
        case AgentState::SWEEPING:   return "Compacting";
        case AgentState::CARRYING:   return "Worktree";
        default:                     return "Unknown";
    }
}
