#include "InksPetWebServer.h"
#include "Logger.h"
#include "MemoryMonitor.h"
#include "ConfigManager.h"
#include "AgentStateManager.h"
#include "PermissionManager.h"
#include "WiFiManager.h"
#include "BatteryManager.h"
#include "version.h"
#include "config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

static const char* TAG = "WebServer";
InksPetWebServer* InksPetWebServer::_instance = nullptr;

InksPetWebServer::InksPetWebServer()
    : _server(nullptr), _ws(nullptr), _initialized(false) {}

InksPetWebServer::~InksPetWebServer() {
    end();
}

InksPetWebServer* InksPetWebServer::getInstance() {
    if (!_instance) _instance = new InksPetWebServer();
    return _instance;
}

bool InksPetWebServer::begin() {
    if (_initialized) return true;

    if (!MemoryMonitor::getInstance()->hasEnoughForWebServer()) {
        LOG_ERROR(TAG, "Insufficient memory for WebServer");
        return false;
    }

    _server = new(std::nothrow) AsyncWebServer(WEBSERVER_PORT);
    if (!_server) {
        LOG_ERROR(TAG, "Failed to create AsyncWebServer");
        return false;
    }

    // CORS headers
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    setupAgentApiRoutes();
    setupConfigApiRoutes();
    setupWiFiApiRoutes();
    setupWebSocket();
    setupStaticFiles();

    // 404 handler
    _server->onNotFound([](AsyncWebServerRequest* request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else if (request->url().startsWith("/api/")) {
            request->send(404, "application/json", "{\"error\":\"endpoint not found\"}");
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });

    _server->begin();
    _initialized = true;
    LOG_INFO(TAG, "WebServer started on port %d", WEBSERVER_PORT);
    return true;
}

void InksPetWebServer::end() {
    if (_server) {
        _server->end();
        delete _server;
        _server = nullptr;
    }
    if (_ws) {
        delete _ws;
        _ws = nullptr;
    }
    _initialized = false;
}

// ---- Agent API Routes ----
void InksPetWebServer::setupAgentApiRoutes() {
    // POST /api/agent/state - Receive agent state events
    _server->on("/api/agent/state", HTTP_POST,
        [](AsyncWebServerRequest* request) { request->send(400); },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<512> doc;
            DeserializationError err = deserializeJson(doc, (const char*)data, len);

            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }

            JsonObject obj = doc.as<JsonObject>();
            String eventType = obj["event"] | "";

            // Handle permission requests specially
            if (eventType == "PermissionRequest") {
                PermissionManager::getInstance()->queueRequest(
                    obj["session"] | obj["agent"].as<String>(),
                    obj["agent"] | "unknown",
                    obj["tool"] | "",
                    obj["file"] | ""
                );
            }

            AgentStateManager::getInstance()->processEvent(obj);

            StaticJsonDocument<128> resp;
            resp["success"] = true;
            resp["state"] = AgentStateManager::stateToString(
                AgentStateManager::getInstance()->getCurrentState());

            String respStr;
            serializeJson(resp, respStr);
            request->send(200, "application/json", respStr);
        }
    );

    // POST /api/agent/permission/response - Permission response
    _server->on("/api/agent/permission/response", HTTP_POST,
        [](AsyncWebServerRequest* request) { request->send(400); },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<256> doc;
            DeserializationError err = deserializeJson(doc, (const char*)data, len);

            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }

            String sessionId = doc["session"] | "";
            String action = doc["action"] | "deny";

            AgentStateManager::getInstance()->respondToPermission(sessionId, action);
            request->send(200, "application/json", "{\"success\":true}");
        }
    );

    // GET /api/agent/status - Query current state
    _server->on("/api/agent/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        auto* asm_ = AgentStateManager::getInstance();
        const AgentSession* session = asm_->getCurrentSession();

        StaticJsonDocument<512> doc;
        doc["state"] = AgentStateManager::stateToString(asm_->getCurrentState());
        doc["active_sessions"] = asm_->getActiveSessionCount();
        doc["uptime"] = millis() / 1000;

        if (session) {
            doc["agent"] = session->agentName;
            doc["session"] = session->sessionId;
            doc["tool"] = session->tool;
            doc["file"] = session->file;
            if (session->hasTasks) {
                JsonObject tasks = doc.createNestedObject("tasks");
                tasks["done"] = session->tasksDone;
                tasks["running"] = session->tasksRunning;
                tasks["pending"] = session->tasksPending;
            }
        }

        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });

    // POST /api/agent/tasks - Update task progress from TodoWrite hook
    _server->on("/api/agent/tasks", HTTP_POST,
        [](AsyncWebServerRequest* request) { request->send(400); },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<256> doc;
            DeserializationError err = deserializeJson(doc, (const char*)data, len);
            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            String session = doc["session"] | doc["agent"].as<String>();
            uint16_t done = doc["done"] | 0;
            uint16_t running = doc["running"] | 0;
            uint16_t pending = doc["pending"] | 0;
            AgentStateManager::getInstance()->updateTasks(session, done, running, pending);
            request->send(200, "application/json", "{\"success\":true}");
        }
    );

    // GET /api/device/info - Device discovery
    _server->on("/api/device/info", HTTP_GET, [](AsyncWebServerRequest* request) {
        StaticJsonDocument<512> doc;
        doc["name"] = FIRMWARE_NAME;
        doc["version"] = VERSION;
        doc["hardware"] = HARDWARE_PLATFORM;
        doc["ip"] = WiFiManager::getInstance()->getIP();
        doc["mac"] = WiFiManager::getInstance()->getMAC();
        doc["heap"] = ESP.getFreeHeap();
        doc["uptime"] = millis() / 1000;

        auto* batt = BatteryManager::getInstance();
        doc["battery_voltage"] = batt->getVoltage();
        doc["battery_percent"] = batt->getPercentage();
        doc["charging"] = batt->isCharging();

        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });
}

// ---- Config API Routes ----
void InksPetWebServer::setupConfigApiRoutes() {
    // GET /api/config - Get all config
    _server->on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
        auto* cfg = ConfigManager::getInstance();
        StaticJsonDocument<512> doc;

        doc["led_brightness"] = cfg->getLedBrightness();
        doc["buzzer_enabled"] = cfg->getBuzzerEnabled();
        doc["buzzer_volume"] = cfg->getBuzzerVolume();
        doc["permission_timeout"] = cfg->getPermissionTimeout();
        doc["permission_default"] = cfg->getPermissionDefault();
        doc["dnd_mode"] = cfg->getDndMode();
        doc["sleep_timeout"] = cfg->getSleepTimeout();
        doc["timezone"] = cfg->getTimezone();
        doc["ntp_server"] = cfg->getNtpServer();
        doc["mdns_hostname"] = cfg->getMdnsHostname();

        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });

    // POST /api/config - Update config
    _server->on("/api/config", HTTP_POST,
        [](AsyncWebServerRequest* request) { request->send(400); },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<512> doc;
            DeserializationError err = deserializeJson(doc, (const char*)data, len);

            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }

            auto* cfg = ConfigManager::getInstance();

            if (doc.containsKey("led_brightness")) cfg->setLedBrightness(doc["led_brightness"]);
            if (doc.containsKey("buzzer_enabled")) cfg->setBuzzerEnabled(doc["buzzer_enabled"]);
            if (doc.containsKey("buzzer_volume")) cfg->setBuzzerVolume(doc["buzzer_volume"]);
            if (doc.containsKey("permission_timeout")) cfg->setPermissionTimeout(doc["permission_timeout"]);
            if (doc.containsKey("permission_default")) cfg->setPermissionDefault(doc["permission_default"]);
            if (doc.containsKey("dnd_mode")) cfg->setDndMode(doc["dnd_mode"]);
            if (doc.containsKey("sleep_timeout")) cfg->setSleepTimeout(doc["sleep_timeout"]);
            if (doc.containsKey("timezone")) cfg->setTimezone(doc["timezone"]);
            if (doc.containsKey("ntp_server")) cfg->setNtpServer(doc["ntp_server"]);
            if (doc.containsKey("mdns_hostname")) cfg->setMdnsHostname(doc["mdns_hostname"]);

            cfg->saveConfig();
            request->send(200, "application/json", "{\"success\":true}");
        }
    );

    // POST /api/config/reset - Factory reset
    _server->on("/api/config/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        ConfigManager::getInstance()->resetToDefaults();
        WiFiManager::getInstance()->clearCredentials();  // Clear WiFi credentials so device boots into AP mode
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Config reset to defaults\"}");
        delay(1000);
        ESP.restart();
    });
}

// ---- WiFi API Routes ----
void InksPetWebServer::setupWiFiApiRoutes() {
    // POST /api/wifi/scan - Scan networks
    _server->on("/api/wifi/scan", HTTP_POST, [](AsyncWebServerRequest* request) {
        WiFi.scanNetworks(true);  // Async scan
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Scanning...\"}");
    });

    // GET /api/wifi/scan - Get scan results
    _server->on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest* request) {
        int n = WiFi.scanComplete();

        if (n == WIFI_SCAN_RUNNING) {
            request->send(200, "application/json", "{\"scanning\":true}");
            return;
        }

        DynamicJsonDocument doc(2048);
        doc["scanning"] = false;
        JsonArray networks = doc.createNestedArray("networks");

        if (n > 0) {
            for (int i = 0; i < n && i < 20; i++) {
                JsonObject net = networks.createNestedObject();
                net["ssid"] = WiFi.SSID(i);
                net["rssi"] = WiFi.RSSI(i);
                net["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
            }
        }

        WiFi.scanDelete();

        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });

    // POST /api/wifi/connect - Connect to network
    _server->on("/api/wifi/connect", HTTP_POST,
        [](AsyncWebServerRequest* request) { request->send(400); },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<256> doc;
            DeserializationError err = deserializeJson(doc, (const char*)data, len);

            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }

            String ssid = doc["ssid"] | "";
            String pass = doc["password"] | "";

            if (ssid.length() == 0) {
                request->send(400, "application/json", "{\"error\":\"SSID required\"}");
                return;
            }

            WiFiManager::getInstance()->saveCredentials(ssid.c_str(), pass.c_str());
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Credentials saved. Restarting...\"}");

            // Restart to apply new WiFi
            delay(1000);
            ESP.restart();
        }
    );

    // Compatibility endpoints
    _server->on("/ip", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", WiFiManager::getInstance()->getIP());
    });

    _server->on("/mac", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", WiFiManager::getInstance()->getMAC());
    });
}

// ---- WebSocket ----
void InksPetWebServer::setupWebSocket() {
    _ws = new AsyncWebSocket("/ws");

    _ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                         AwsEventType type, void* arg, uint8_t* data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                LOG_INFO(TAG, "WebSocket client #%u connected", client->id());
                sendDeviceInfo(client);
                break;
            case WS_EVT_DISCONNECT:
                LOG_INFO(TAG, "WebSocket client #%u disconnected", client->id());
                break;
            case WS_EVT_DATA: {
                AwsFrameInfo* info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                    char* msg = new char[len + 1];
                    memcpy(msg, data, len);
                    msg[len] = '\0';
                    handleWebSocketMessage(client, msg);
                    delete[] msg;
                }
                break;
            }
            default:
                break;
        }
    });

    _server->addHandler(_ws);
}

void InksPetWebServer::handleWebSocketMessage(AsyncWebSocketClient* client, const char* data) {
    StaticJsonDocument<200> doc;
    DeserializationError err = deserializeJson(doc, data);
    if (err) return;

    const char* type = doc["type"];
    if (!type) return;

    if (strcmp(type, "ping") == 0) {
        client->text("{\"type\":\"pong\"}");
    } else if (strcmp(type, "requestDeviceInfo") == 0) {
        sendDeviceInfo(client);
    }
}

void InksPetWebServer::sendDeviceInfo(AsyncWebSocketClient* client) {
    StaticJsonDocument<512> doc;
    doc["type"] = "deviceInfo";
    doc["ip"] = WiFiManager::getInstance()->getIP();
    doc["mac"] = WiFiManager::getInstance()->getMAC();
    doc["ssid"] = WiFiManager::getInstance()->getSSID();
    doc["rssi"] = WiFiManager::getInstance()->getRSSI();
    doc["heap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;
    doc["version"] = VERSION;
    doc["state"] = AgentStateManager::stateToString(
        AgentStateManager::getInstance()->getCurrentState());
    doc["active_sessions"] = AgentStateManager::getInstance()->getActiveSessionCount();

    String resp;
    serializeJson(doc, resp);
    client->text(resp);
}

void InksPetWebServer::broadcastState() {
    if (!_ws || _ws->count() == 0) return;

    StaticJsonDocument<256> doc;
    doc["type"] = "stateUpdate";
    doc["state"] = AgentStateManager::stateToString(
        AgentStateManager::getInstance()->getCurrentState());
    doc["active_sessions"] = AgentStateManager::getInstance()->getActiveSessionCount();

    const AgentSession* session = AgentStateManager::getInstance()->getCurrentSession();
    if (session) {
        doc["agent"] = session->agentName;
        doc["tool"] = session->tool;
    }

    String resp;
    serializeJson(doc, resp);
    _ws->textAll(resp);
}

// ---- Static Files ----
void InksPetWebServer::setupStaticFiles() {
    if (!LittleFS.begin()) {
        LOG_ERROR(TAG, "Failed to mount LittleFS");
        return;
    }

    // Serve config page
    _server->on("/config", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!LittleFS.exists("/config.html")) {
            request->send(404, "text/plain", "Config page not found");
            return;
        }
        request->send(LittleFS, "/config.html", "text/html");
    });

    // Serve static files from LittleFS
    _server->serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("max-age=600");

    LOG_INFO(TAG, "Static files configured from LittleFS");
}
