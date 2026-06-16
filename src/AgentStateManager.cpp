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

static void updateStringField(JsonObject obj, const char* key, String& target) {
    if (obj.containsKey(key)) target = obj[key].as<String>();
}

static void updateStringField(JsonVariantConst value, String& target) {
    if (!value.isNull()) target = value.as<String>();
}

static bool sameNonEmpty(const String& a, const String& b) {
    return a.length() && b.length() && a == b;
}

static bool looksLikeCommandTitle(const String& value) {
    String v = value;
    v.trim();
    if (!v.length()) return false;
    if (v.startsWith("$ ")) return true;
    String lower = v;
    lower.toLowerCase();
    const char* prefixes[] = {
        "git ", "node ", "npm ", "pnpm ", "yarn ", "python ", "python3 ",
        "bash ", "sh ", "zsh ", "curl ", "pio ", "make ", "cmake ",
        "cargo ", "go ", "rsync ", "mkdir ", "rm ", "cp ", "mv ",
        "apply_patch", "sed ", "rg ", "grep ", "cat ", "ls ", "find "
    };
    for (const char* prefix : prefixes) {
        if (lower.startsWith(prefix)) return true;
    }
    return false;
}

static void updateTitleField(JsonVariantConst value, String& target,
                             const String& repo, const String& action,
                             const String& tool, const String& file) {
    if (value.isNull()) return;
    String candidate = value.as<String>();
    candidate.trim();
    if (!candidate.length()) return;
    if (candidate == "Waiting for next step" && target.length()) return;
    if (sameNonEmpty(candidate, repo) || sameNonEmpty(candidate, action) ||
        sameNonEmpty(candidate, tool) || sameNonEmpty(candidate, file) ||
        looksLikeCommandTitle(candidate)) {
        return;
    }
    target = candidate;
}

static String buildUsageLine(JsonObject window) {
    String label = window["label"] | window["window"] | window["period"] | "";
    int percent = window["percent"] | window["pct"] | window["usedPercent"] | window["used_percent"] | -1;
    String remain = window["remaining"] | window["resetIn"] | window["reset_in"] | "";
    String line = label;
    if (percent >= 0) {
        if (line.length()) line += " ";
        line += String(percent) + "%";
    }
    if (remain.length()) {
        if (line.length()) line += " ";
        line += remain;
    }
    return line;
}

static String taskItemText(JsonVariant item) {
    if (item.is<const char*>()) return item.as<String>();
    JsonObject obj = item.as<JsonObject>();
    if (obj.isNull()) return String();
    const char* keys[] = {"title", "content", "text", "summary", "action"};
    for (const char* key : keys) {
        if (!obj.containsKey(key)) continue;
        String value = obj[key].as<String>();
        value.trim();
        if (value.length()) return value;
    }
    return String();
}

static void updateTaskItems(JsonArray items, AgentSession* session) {
    if (!session || items.isNull()) return;
    session->taskItemCount = 0;
    for (uint8_t i = 0; i < AgentSession::MAX_TASK_ITEMS; i++) {
        session->taskItems[i] = "";
    }
    for (JsonVariant item : items) {
        String text = taskItemText(item);
        text.trim();
        if (!text.length()) continue;
        session->taskItems[session->taskItemCount++] = text;
        if (session->taskItemCount >= AgentSession::MAX_TASK_ITEMS) break;
    }
    if (session->taskItemCount > 0) session->hasTasks = true;
}

static void updateQuestionOptions(JsonArray options, AgentSession* session) {
    if (!session || options.isNull()) return;
    session->questionOptionCount = 0;
    for (uint8_t i = 0; i < AgentSession::MAX_TASK_ITEMS; i++) {
        session->questionOptions[i] = "";
    }
    for (JsonVariant item : options) {
        String text = taskItemText(item);
        text.trim();
        if (!text.length()) continue;
        session->questionOptions[session->questionOptionCount++] = text;
        if (session->questionOptionCount >= AgentSession::MAX_TASK_ITEMS) break;
    }
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
    session->priority = event["priority"] | session->priority;

    updateStringField(event, "summary", session->taskSummary);
    updateStringField(event, "promptSnippet", session->promptSnippet);
    updateStringField(event, "prompt", session->promptSnippet);
    updateStringField(event, "repo", session->repo);
    updateStringField(event, "branch", session->branch);
    updateStringField(event, "action", session->currentAction);
    updateStringField(event, "language", session->language);
    updateStringField(event, "error", session->errorMessage);
    updateStringField(event, "message", session->errorMessage);
    updateTitleField(event["title"], session->taskTitle, session->repo,
                     session->currentAction, session->tool, session->file);

    JsonObject task = event["task"].as<JsonObject>();
    if (!task.isNull()) {
        updateStringField(task, "repo", session->repo);
        updateStringField(task, "branch", session->branch);
        updateStringField(task, "current", session->currentAction);
        updateStringField(task, "action", session->currentAction);
        updateTitleField(task["title"], session->taskTitle, session->repo,
                         session->currentAction, session->tool, session->file);
        updateStringField(task, "summary", session->taskSummary);
        updateStringField(task, "promptSnippet", session->promptSnippet);
        updateTaskItems(task["items"].as<JsonArray>(), session);
    }

    JsonObject tasks = event["tasks"].as<JsonObject>();
    if (!tasks.isNull()) {
        session->tasksDone = tasks["done"] | session->tasksDone;
        session->tasksRunning = tasks["running"] | session->tasksRunning;
        session->tasksPending = tasks["pending"] | session->tasksPending;
        session->hasTasks = true;
        updateTaskItems(tasks["items"].as<JsonArray>(), session);
    }

    JsonObject display = event["display"].as<JsonObject>();
    if (!display.isNull()) {
        updateStringField(display, "language", session->language);
    }

    JsonObject question = event["question"].as<JsonObject>();
    if (!question.isNull()) {
        updateStringField(question, "text", session->taskSummary);
        updateQuestionOptions(question["options"].as<JsonArray>(), session);
    }
    updateQuestionOptions(event["options"].as<JsonArray>(), session);

    JsonObject recent = event["recentCompletion"].as<JsonObject>();
    if (!recent.isNull()) {
        updateStringField(recent, "title", session->recentCompletionTitle);
        updateStringField(recent, "ago", session->recentCompletionAgo);
    }

    JsonObject permission = event["permission"].as<JsonObject>();
    if (!permission.isNull()) {
        updateStringField(permission, "command", session->permissionCommand);
    }

    JsonObject progress = event["progress"].as<JsonObject>();
    if (!progress.isNull()) {
        session->hasReliableProgress = progress["reliable"] | progress["hasReliableProgress"] | session->hasReliableProgress;
        session->progressDone = progress["done"] | session->progressDone;
        session->progressTotal = progress["total"] | session->progressTotal;
        if (session->progressTotal > 0) {
            session->tasksDone = session->progressDone;
            session->tasksRunning = (session->progressDone < session->progressTotal) ? 1 : 0;
            session->tasksPending = (session->progressDone < session->progressTotal)
                                  ? (session->progressTotal - session->progressDone - session->tasksRunning)
                                  : 0;
            session->hasTasks = true;
        }
    }

    JsonObject usage = event["usage"].as<JsonObject>();
    if (!usage.isNull()) {
        JsonArray windows = usage["windows"].as<JsonArray>();
        if (!windows.isNull()) {
            session->usageLine1 = "";
            session->usageLine2 = "";
            uint8_t idx = 0;
            for (JsonObject window : windows) {
                String line = buildUsageLine(window);
                if (!line.length()) continue;
                if (idx == 0) session->usageLine1 = line;
                else if (idx == 1) session->usageLine2 = line;
                idx++;
                if (idx >= 2) break;
            }
            session->hasUsage = session->usageLine1.length() || session->usageLine2.length();
        }
    }

    if (!session->currentAction.length() && tool.length()) {
        session->currentAction = tool;
        if (file.length()) {
            session->currentAction += " ";
            session->currentAction += file;
        }
    }

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
    String explicitState = event["state"] | "";
    AgentState newState = explicitState.length()
                         ? stateFromString(explicitState, mapEventToState(eventType, activeSessions))
                         : mapEventToState(eventType, activeSessions);
    session->state = newState;

    // Handle Stop event - mark as completed
    if (eventType == "Stop") {
        session->state = AgentState::COMPLETED;
        if (session->taskTitle.length()) {
            session->recentCompletionTitle = session->taskTitle;
            session->recentCompletionAgo = "just now";
        }
    }

    notifyStateChange();
}

void AgentStateManager::updateTasks(const String& sessionId, uint16_t done, uint16_t running, uint16_t pending,
                                    const String* items, uint8_t itemCount) {
    // Update matching session OR the highest-priority active one if sessionId not found
    AgentSession* target = nullptr;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (_sessions[i].active && _sessions[i].sessionId == sessionId) {
            target = &_sessions[i];
            break;
        }
    }

    // Fallback: apply to any active session (agents may not always send consistent session IDs)
    if (!target) {
        for (int i = 0; i < MAX_SESSIONS; i++) {
            if (_sessions[i].active) {
                target = &_sessions[i];
                break;
            }
        }
    }

    if (target) {
        target->tasksDone = done;
        target->tasksRunning = running;
        target->tasksPending = pending;
        target->hasTasks = true;
        target->hasReliableProgress = true;
        target->progressDone = done;
        target->progressTotal = done + running + pending;
        target->taskItemCount = 0;
        for (uint8_t i = 0; i < AgentSession::MAX_TASK_ITEMS; i++) {
            target->taskItems[i] = "";
        }
        for (uint8_t i = 0; items && i < itemCount && i < AgentSession::MAX_TASK_ITEMS; i++) {
            String text = items[i];
            text.trim();
            if (!text.length()) continue;
            target->taskItems[target->taskItemCount++] = text;
        }
        target->lastUpdate = millis();
        LOG_INFO(TAG, "Tasks updated: %u done, %u running, %u pending", done, running, pending);
        notifyStateChange();
    } else {
        LOG_WARNING(TAG, "updateTasks: no active session found for %s", sessionId.c_str());
    }
}

AgentState AgentStateManager::mapEventToState(const String& event, int activeSessions) {
    if (event == "PermissionRequest") return AgentState::PERMISSION;
    if (event == "Ask" || event == "Question" || event == "UserInputRequest") return AgentState::ASK;
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
            if (!best ||
                _sessions[i].priority > best->priority ||
                (_sessions[i].priority == best->priority &&
                 _sessions[i].lastUpdate > best->lastUpdate)) {
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
    bool changed = false;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!_sessions[i].active) continue;

        unsigned long timeout = SLEEP_TIMEOUT_MS;
        if (_sessions[i].state == AgentState::ERROR) timeout = 5000;
        if (_sessions[i].state == AgentState::COMPLETED) timeout = 15000;

        if (now - _sessions[i].lastUpdate > timeout) {
            LOG_INFO(TAG, "Session %s timed out", _sessions[i].sessionId.c_str());
            _sessions[i].active = false;
            changed = true;
        }
    }
    if (changed) notifyStateChange();
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
            resetSessionFields(&_sessions[i], agent, session);
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
    resetSessionFields(&_sessions[oldest], agent, session);
    return &_sessions[oldest];
}

void AgentStateManager::notifyStateChange() {
    AgentState current = getCurrentState();
    if (current != _lastReportedState) {
        _lastReportedState = current;
    }
    if (_callback) _callback(current, getCurrentSession());
}

void AgentStateManager::resetSessionFields(AgentSession* session, const String& agent, const String& sessionId) {
    if (!session) return;
    session->agentName = agent;
    session->sessionId = sessionId;
    session->state = AgentState::IDLE;
    session->tool = "";
    session->file = "";
    session->taskTitle = "";
    session->taskSummary = "";
    session->promptSnippet = "";
    session->repo = "";
    session->branch = "";
    session->currentAction = "";
    session->language = "";
    session->usageLine1 = "";
    session->usageLine2 = "";
    session->errorMessage = "";
    session->recentCompletionTitle = "";
    session->recentCompletionAgo = "";
    session->permissionCommand = "";
    session->lastUpdate = millis();
    session->sessionStart = millis();
    session->active = true;
    session->priority = 0;
    session->toolCalls = 0;
    session->reads = 0;
    session->writes = 0;
    session->edits = 0;
    session->bashes = 0;
    session->tasksDone = 0;
    session->tasksRunning = 0;
    session->tasksPending = 0;
    session->hasTasks = false;
    session->taskItemCount = 0;
    session->hasReliableProgress = false;
    session->progressDone = 0;
    session->progressTotal = 0;
    session->hasUsage = false;
    session->questionOptionCount = 0;
    for (uint8_t i = 0; i < AgentSession::MAX_TASK_ITEMS; i++) {
        session->taskItems[i] = "";
        session->questionOptions[i] = "";
    }
}

const char* AgentStateManager::stateToString(AgentState state) {
    switch (state) {
        case AgentState::SLEEPING:   return "sleeping";
        case AgentState::IDLE:       return "idle";
        case AgentState::THINKING:   return "thinking";
        case AgentState::WORKING:    return "working";
        case AgentState::ERROR:      return "error";
        case AgentState::ASK:        return "ask";
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
        case AgentState::ASK:        return "Question?";
        case AgentState::COMPLETED:  return "Done!";
        case AgentState::PERMISSION: return "Permission?";
        case AgentState::JUGGLING:   return "Juggling";
        case AgentState::CONDUCTING: return "Conducting";
        case AgentState::SWEEPING:   return "Compacting";
        case AgentState::CARRYING:   return "Worktree";
        default:                     return "Unknown";
    }
}

AgentState AgentStateManager::stateFromString(const String& state, AgentState fallback) {
    if (state == "sleeping") return AgentState::SLEEPING;
    if (state == "idle") return AgentState::IDLE;
    if (state == "thinking") return AgentState::THINKING;
    if (state == "working") return AgentState::WORKING;
    if (state == "error") return AgentState::ERROR;
    if (state == "ask" || state == "question") return AgentState::ASK;
    if (state == "completed" || state == "done") return AgentState::COMPLETED;
    if (state == "permission") return AgentState::PERMISSION;
    if (state == "juggling") return AgentState::JUGGLING;
    if (state == "conducting" || state == "multi") return AgentState::CONDUCTING;
    if (state == "sweeping") return AgentState::SWEEPING;
    if (state == "carrying") return AgentState::CARRYING;
    return fallback;
}
