#include "PermissionManager.h"
#include "ConfigManager.h"
#include "Logger.h"

static const char* TAG = "Permission";
PermissionManager* PermissionManager::_instance = nullptr;

PermissionManager::PermissionManager() : _pendingCount(0), _responseCallback(nullptr) {
    for (int i = 0; i < MAX_QUEUE; i++) {
        _queue[i].active = false;
    }
}

PermissionManager* PermissionManager::getInstance() {
    if (!_instance) _instance = new PermissionManager();
    return _instance;
}

void PermissionManager::queueRequest(const String& sessionId, const String& agent,
                                      const String& tool, const String& file) {
    // Find empty slot
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (!_queue[i].active) {
            _queue[i].sessionId = sessionId;
            _queue[i].agent = agent;
            _queue[i].tool = tool;
            _queue[i].file = file;
            _queue[i].timestamp = millis();
            _queue[i].active = true;
            _pendingCount++;
            LOG_INFO(TAG, "Queued permission: %s wants %s on %s",
                     agent.c_str(), tool.c_str(), file.c_str());
            return;
        }
    }

    // Queue full - auto deny oldest
    LOG_WARNING(TAG, "Permission queue full, auto-denying oldest");
    _queue[0].active = false;
    _pendingCount--;

    // Shift queue forward
    for (int i = 0; i < MAX_QUEUE - 1; i++) {
        _queue[i] = _queue[i + 1];
    }
    _queue[MAX_QUEUE - 1].sessionId = sessionId;
    _queue[MAX_QUEUE - 1].agent = agent;
    _queue[MAX_QUEUE - 1].tool = tool;
    _queue[MAX_QUEUE - 1].file = file;
    _queue[MAX_QUEUE - 1].timestamp = millis();
    _queue[MAX_QUEUE - 1].active = true;
    _pendingCount++;
}

void PermissionManager::handleAllow() { respond("allow"); }
void PermissionManager::handleAlwaysAllow() { respond("always_allow"); }
void PermissionManager::handleDeny() { respond("deny"); }

void PermissionManager::respond(const String& action) {
    if (_pendingCount <= 0) return;

    // Find first active request
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (_queue[i].active) {
            LOG_INFO(TAG, "Response: %s for session %s",
                     action.c_str(), _queue[i].sessionId.c_str());

            if (_responseCallback) {
                _responseCallback(_queue[i].sessionId, action);
            }

            _queue[i].active = false;
            _pendingCount--;
            advanceQueue();
            break;
        }
    }
}

void PermissionManager::advanceQueue() {
    // Compact the queue so active items are at the front
    int writeIdx = 0;
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (_queue[i].active) {
            if (i != writeIdx) {
                _queue[writeIdx] = _queue[i];
                _queue[i].active = false;
            }
            writeIdx++;
        }
    }
}

void PermissionManager::update() {
    if (_pendingCount <= 0) return;

    int timeoutSec = ConfigManager::getInstance()->getPermissionTimeout();
    unsigned long timeoutMs = static_cast<unsigned long>(timeoutSec) * 1000;

    for (int i = 0; i < MAX_QUEUE; i++) {
        if (_queue[i].active && (millis() - _queue[i].timestamp > timeoutMs)) {
            String defaultAction = ConfigManager::getInstance()->getPermissionDefault();
            LOG_WARNING(TAG, "Permission timeout for session %s, auto-%s",
                       _queue[i].sessionId.c_str(), defaultAction.c_str());

            if (_responseCallback) {
                _responseCallback(_queue[i].sessionId, defaultAction);
            }

            _queue[i].active = false;
            _pendingCount--;
        }
    }
}

const char* PermissionManager::getCurrentAgent() const {
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (_queue[i].active) return _queue[i].agent.c_str();
    }
    return "";
}

const char* PermissionManager::getCurrentTool() const {
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (_queue[i].active) return _queue[i].tool.c_str();
    }
    return "";
}

const char* PermissionManager::getCurrentFile() const {
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (_queue[i].active) return _queue[i].file.c_str();
    }
    return "";
}

const char* PermissionManager::getCurrentSession() const {
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (_queue[i].active) return _queue[i].sessionId.c_str();
    }
    return "";
}
