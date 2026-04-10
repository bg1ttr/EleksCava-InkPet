#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <DNSServer.h>

enum class WiFiStatus {
    UNKNOWN,
    CONNECTED,
    CONNECTING,
    DISCONNECTED,
    CONNECT_FAILED,
    AP_MODE
};

class WiFiManager {
public:
    static WiFiManager* getInstance();
    static bool hasWiFiConfig();

    bool begin();
    bool autoConnect();
    bool connect(const char* ssid, const char* pass);
    void startAPMode();
    void stopAPMode();
    void loop();  // Call in main loop for AP mode DNS handling

    bool isConnected() const;
    bool isAPMode() const { return _apMode; }
    WiFiStatus getStatus() const;
    String getIP() const;
    String getMAC() const;
    String getSSID() const;
    int getRSSI() const;
    String getAPSSID() const { return _apSSID; }

    bool saveCredentials(const char* ssid, const char* pass);
    void clearCredentials();

    using ConnectedCallback = std::function<void()>;
    void onConnected(ConnectedCallback cb) { _connectedCb = cb; }

private:
    WiFiManager();
    static WiFiManager* _instance;

    Preferences _prefs;
    DNSServer _dnsServer;
    bool _apMode;
    String _apSSID;
    ConnectedCallback _connectedCb;

    String generateAPSSID();
    bool loadAndConnect();
};
