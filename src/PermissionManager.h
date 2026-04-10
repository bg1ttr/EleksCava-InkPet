#pragma once
#include <Arduino.h>
#include <functional>
#include "config.h"

class PermissionManager {
public:
    static PermissionManager* getInstance();

    // Queue a permission request
    void queueRequest(const String& sessionId, const String& agent,
                      const String& tool, const String& file);

    // Handle button press response
    void handleAllow();
    void handleAlwaysAllow();
    void handleDeny();

    // Check for timeout
    void update();

    bool hasPending() const { return _pendingCount > 0; }
    int getPendingCount() const { return _pendingCount; }

    // Get current request details for display
    const char* getCurrentAgent() const;
    const char* getCurrentTool() const;
    const char* getCurrentFile() const;
    const char* getCurrentSession() const;

    // Callback for when a response is ready to send
    using ResponseCallback = std::function<void(const String& sessionId, const String& action)>;
    void onResponse(ResponseCallback cb) { _responseCallback = cb; }

private:
    PermissionManager();
    static PermissionManager* _instance;

    struct PermissionRequest {
        String sessionId;
        String agent;
        String tool;
        String file;
        unsigned long timestamp;
        bool active;
    };

    static const int MAX_QUEUE = MAX_PERMISSION_QUEUE;
    PermissionRequest _queue[MAX_PERMISSION_QUEUE];
    int _pendingCount;
    ResponseCallback _responseCallback;

    void respond(const String& action);
    void advanceQueue();
};
