#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

class InksPetWebServer {
public:
    static InksPetWebServer* getInstance();
    bool begin();
    void end();
    AsyncWebServer* getServer() { return _server; }

private:
    InksPetWebServer();
    ~InksPetWebServer();
    static InksPetWebServer* _instance;

    AsyncWebServer* _server;
    AsyncWebSocket* _ws;
    bool _initialized;

    void setupAgentApiRoutes();
    void setupConfigApiRoutes();
    void setupWiFiApiRoutes();
    void setupStaticFiles();
    void setupWebSocket();

    void handleWebSocketMessage(AsyncWebSocketClient* client, const char* data);
    void sendDeviceInfo(AsyncWebSocketClient* client);
    void broadcastState();
};
