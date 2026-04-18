#pragma once

// BuddyProtocol — JSON line dispatcher for the Claude desktop buddy protocol.
// Sits between BleBridge (raw line transport) and the domain layer
// (BuddyStateMapper, PermissionManager, FileXfer, BuddyStats).

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

class BuddyProtocol {
public:
    static BuddyProtocol* getInstance();

    void begin();

    // Fed by BleBridge.onLine(): a single UTF-8 JSON object, no trailing \n.
    void feedLine(const char* line, size_t len);

    // Emit helpers — write JSON line out over BLE.
    void sendAck(const char* what, bool ok, uint32_t n = 0, const char* error = nullptr);
    void sendPermissionDecision(const char* id, const char* decision);
    void sendStatusAck();

    // Liveness (mirrors official dataConnected — last inbound byte ≤ 30s ago).
    bool isConnected() const;
    uint32_t lastInboundMs() const { return _lastInboundMs; }

    // Time sync (epoch_sec + tz_offset_sec) → TimeManager RTC.
    bool hasTimeSync() const { return _timeSynced; }

private:
    BuddyProtocol();
    static BuddyProtocol* _instance;

    void _handleSnapshot(JsonDocument& doc);
    void _handleTurn(JsonDocument& doc);
    void _handleCommand(JsonDocument& doc);  // cmd field routing
    void _handleTime(JsonDocument& doc);

    uint32_t _lastInboundMs;
    bool     _timeSynced;
    // Retain the latest tokens_today to distinguish cumulative bumps.
    uint32_t _lastTokensTotal;
};
