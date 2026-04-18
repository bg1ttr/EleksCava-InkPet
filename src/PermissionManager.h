#pragma once
#include <Arduino.h>
#include <functional>
#include "config.h"

class PermissionManager {
public:
    // Where the request came from — drives which reply channel we use
    // when the user presses a button. BLE prompts need {cmd:permission}
    // back over NUS; HTTP prompts need the agent-state webhook callback.
    enum class Source : uint8_t {
        HTTP = 0,   // Claude Code hook via POST /api/agent/state
        BLE  = 1,   // Claude Desktop snapshot.prompt over NUS
    };

    static PermissionManager* getInstance();

    // Queue a permission request
    void queueRequest(const String& sessionId, const String& agent,
                      const String& tool, const String& file,
                      Source source = Source::HTTP);

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

    // Callback for when a response is ready to send. The source arg lets
    // the main firmware route the reply back to the right channel.
    using ResponseCallback = std::function<void(const String& sessionId,
                                                const String& action,
                                                Source source)>;
    void onResponse(ResponseCallback cb) { _responseCallback = cb; }

    // Source of the currently-displayed (head-of-queue) request.
    Source getCurrentSource() const;

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
        Source source;
    };

    static const int MAX_QUEUE = MAX_PERMISSION_QUEUE;
    PermissionRequest _queue[MAX_PERMISSION_QUEUE];
    int _pendingCount;
    ResponseCallback _responseCallback;

    void respond(const String& action);
    void advanceQueue();
};
