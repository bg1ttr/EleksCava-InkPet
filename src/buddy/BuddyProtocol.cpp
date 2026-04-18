#include "BuddyProtocol.h"
#include <ArduinoJson.h>
#include <string.h>
#include "BleBridge.h"
#include "BuddyStateMapper.h"
#include "BuddyStats.h"
#include "FileXfer.h"
#include "../Logger.h"
#include "../TimeManager.h"
#include "../BatteryManager.h"
#include "../version.h"

static const char* TAG = "Buddy";
BuddyProtocol* BuddyProtocol::_instance = nullptr;

BuddyProtocol::BuddyProtocol()
    : _lastInboundMs(0), _timeSynced(false), _lastTokensTotal(0) {}

BuddyProtocol* BuddyProtocol::getInstance() {
    if (!_instance) _instance = new BuddyProtocol();
    return _instance;
}

void BuddyProtocol::begin() {
    // Nothing to init eagerly — callers wire BleBridge.onLine(...) to feedLine().
}

bool BuddyProtocol::isConnected() const {
    if (!_lastInboundMs) return false;
    return (millis() - _lastInboundMs) <= 30000;
}

// ============================================================
// Inbound JSON dispatch
// ============================================================

void BuddyProtocol::feedLine(const char* line, size_t len) {
    _lastInboundMs = millis();

    // A single line may be a snapshot, a turn event, or a command.
    // Snapshots don't carry a "cmd"; commands and turn events do.
    DynamicJsonDocument doc(4096);  // upstream caps turn events at 4KB on the wire
    DeserializationError err = deserializeJson(doc, line, len);
    if (err) {
        LOG_WARNING(TAG, "JSON parse error: %s", err.c_str());
        return;
    }

    // File-push commands get first dibs — while a transfer is in flight every
    // chunk must be acked fast and we don't want to parse as a snapshot.
    if (FileXfer::getInstance()->tryHandle(doc)) return;

    if (doc.containsKey("cmd")) {
        _handleCommand(doc);
        return;
    }
    if (doc.containsKey("evt")) {
        _handleTurn(doc);
        return;
    }
    if (doc.containsKey("time")) {
        _handleTime(doc);
        // fall through — "time" can co-arrive with a snapshot
    }

    // Otherwise treat as a snapshot (has total/running/waiting/msg fields).
    if (doc.containsKey("total") || doc.containsKey("running") ||
        doc.containsKey("waiting") || doc.containsKey("msg") ||
        doc.containsKey("prompt")) {
        _handleSnapshot(doc);
    }
}

void BuddyProtocol::_handleSnapshot(JsonDocument& doc) {
    BuddySnapshot s;
    memset(&s, 0, sizeof(s));

    s.total   = (uint8_t)(doc["total"]   | 0);
    s.running = (uint8_t)(doc["running"] | 0);
    s.waiting = (uint8_t)(doc["waiting"] | 0);
    s.recentlyCompleted = doc["completed"] | false;
    s.tokens      = (uint32_t)(doc["tokens"]       | 0);
    s.tokensToday = (uint32_t)(doc["tokens_today"] | 0);
    s.connected   = true;

    const char* msg = doc["msg"] | "";
    strncpy(s.msg, msg, sizeof(s.msg) - 1);

    // prompt {id, tool, hint}
    JsonVariant p = doc["prompt"];
    if (p.is<JsonObject>()) {
        JsonObject po = p.as<JsonObject>();
        strncpy(s.promptId,   po["id"]   | "",  sizeof(s.promptId)   - 1);
        strncpy(s.promptTool, po["tool"] | "",  sizeof(s.promptTool) - 1);
        strncpy(s.promptHint, po["hint"] | "",  sizeof(s.promptHint) - 1);
    }

    // entries — last 4 transcript lines
    JsonVariant e = doc["entries"];
    if (e.is<JsonArray>()) {
        JsonArray arr = e.as<JsonArray>();
        uint8_t n = 0;
        for (JsonVariant v : arr) {
            if (n >= 4) break;
            const char* txt = v | "";
            strncpy(s.lines[n], txt, sizeof(s.lines[n]) - 1);
            n++;
        }
        s.nLines = n;
    }

    // Token bookkeeping for level tracking
    if (s.tokens > 0) BuddyStats::getInstance()->onBridgeTokens(s.tokens);

    LOG_INFO(TAG, "Snap: t=%u r=%u w=%u today=%u msg='%s' lines=%u prompt='%s'",
             s.total, s.running, s.waiting, s.tokensToday,
             s.msg, s.nLines, s.promptId);

    BuddyStateMapper::getInstance()->applySnapshot(s);
}

void BuddyProtocol::_handleTurn(JsonDocument& doc) {
    // Turn events carry the raw SDK content array. We don't render long
    // transcripts on the e-paper, but we log for future surfaces.
    const char* role = doc["role"] | "assistant";
    LOG_DEBUG(TAG, "Turn event role=%s", role);
    // content[] could be inspected here for tool_use summaries in future.
}

void BuddyProtocol::_handleTime(JsonDocument& doc) {
    JsonVariant arr = doc["time"];
    if (!arr.is<JsonArray>() || arr.size() < 2) return;
    uint32_t epoch = (uint32_t)(arr[0].as<uint32_t>());
    int32_t  tz    = arr[1].as<int32_t>();
    if (epoch > 1700000000UL) {
        TimeManager::getInstance()->setTimeFromEpoch(epoch, tz);
        _timeSynced = true;
        LOG_INFO(TAG, "Time synced from desktop: epoch=%u tz=%d", epoch, tz);
    }
}

void BuddyProtocol::_handleCommand(JsonDocument& doc) {
    const char* cmd = doc["cmd"];
    if (!cmd) return;
    LOG_INFO(TAG, "cmd=%s", cmd);

    if (strcmp(cmd, "name") == 0) {
        const char* n = doc["name"] | "";
        BuddyStats::getInstance()->setPetName(n);
        sendAck("name", true);
        return;
    }
    if (strcmp(cmd, "owner") == 0) {
        const char* n = doc["name"] | "";
        BuddyStats::getInstance()->setOwner(n);
        sendAck("owner", true);
        return;
    }
    if (strcmp(cmd, "unpair") == 0) {
        BleBridge::getInstance()->clearBonds();
        sendAck("unpair", true);
        return;
    }
    if (strcmp(cmd, "status") == 0) {
        sendStatusAck();
        return;
    }
    if (strcmp(cmd, "species") == 0) {
        uint8_t idx = (uint8_t)(doc["idx"] | 0xFF);
        BuddyStats::getInstance()->setSpecies(idx);
        sendAck("species", true);
        return;
    }
    // char_begin / file / chunk / file_end / char_end are handled by FileXfer
    // earlier in feedLine(). Anything else is acked as not-supported.
    LOG_WARNING(TAG, "Unknown cmd: %s", cmd);
    sendAck(cmd, false, 0, "unsupported");
}

// ============================================================
// Outbound
// ============================================================

void BuddyProtocol::sendAck(const char* what, bool ok, uint32_t n, const char* error) {
    char buf[160];
    int len;
    if (error && !ok) {
        len = snprintf(buf, sizeof(buf),
            "{\"ack\":\"%s\",\"ok\":false,\"n\":%lu,\"error\":\"%s\"}\n",
            what, (unsigned long)n, error);
    } else {
        len = snprintf(buf, sizeof(buf),
            "{\"ack\":\"%s\",\"ok\":%s,\"n\":%lu}\n",
            what, ok ? "true" : "false", (unsigned long)n);
    }
    if (len > 0) {
        BleBridge::getInstance()->write(reinterpret_cast<const uint8_t*>(buf), len);
    }
}

void BuddyProtocol::sendPermissionDecision(const char* id, const char* decision) {
    char buf[128];
    int len = snprintf(buf, sizeof(buf),
        "{\"cmd\":\"permission\",\"id\":\"%s\",\"decision\":\"%s\"}\n",
        id, decision);
    if (len > 0) {
        BleBridge::getInstance()->write(reinterpret_cast<const uint8_t*>(buf), len);
        LOG_INFO(TAG, "Sent permission decision: %s for %s", decision, id);
    }
}

void BuddyProtocol::sendStatusAck() {
    // Build a status response that echoes the upstream shape.
    // Fields InksPet cannot provide are omitted (the spec allows this):
    //   bat.mA (no current sensor), stats.vel/nap come from BuddyStats.
    DynamicJsonDocument doc(512);
    doc["ack"] = "status";
    doc["ok"]  = true;
    JsonObject data = doc.createNestedObject("data");

    const char* pn = BuddyStats::getInstance()->getPetName();
    data["name"]    = pn[0] ? pn : BleBridge::getInstance()->getDeviceName();
    data["sec"]     = BleBridge::getInstance()->isSecured();
    data["fw"]      = VERSION;

    // Battery
    BatteryManager* bat = BatteryManager::getInstance();
    JsonObject b = data.createNestedObject("bat");
    b["pct"] = bat->getPercentage();
    b["mV"]  = (int)(bat->getVoltage() * 1000.0f);
    b["usb"] = bat->isCharging();
    // mA omitted — no shunt.

    // System
    JsonObject sys = data.createNestedObject("sys");
    sys["up"]   = (uint32_t)(millis() / 1000);
    sys["heap"] = (uint32_t)ESP.getFreeHeap();

    // Stats
    BuddyStats* st = BuddyStats::getInstance();
    JsonObject stats = data.createNestedObject("stats");
    stats["appr"] = st->getApprovals();
    stats["deny"] = st->getDenials();
    stats["vel"]  = st->getMedianVelocity();
    stats["nap"]  = st->getNapSeconds();
    stats["lvl"]  = st->getLevel();

    char out[600];
    size_t written = serializeJson(doc, out, sizeof(out) - 1);
    out[written++] = '\n';
    BleBridge::getInstance()->write(reinterpret_cast<const uint8_t*>(out), written);
}
