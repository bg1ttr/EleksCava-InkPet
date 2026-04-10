#include "WiFiManager.h"
#include "Logger.h"
#include "config.h"

static const char* TAG = "WiFi";
WiFiManager* WiFiManager::_instance = nullptr;

WiFiManager::WiFiManager() : _apMode(false), _connectedCb(nullptr) {}

WiFiManager* WiFiManager::getInstance() {
    if (!_instance) _instance = new WiFiManager();
    return _instance;
}

bool WiFiManager::hasWiFiConfig() {
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE_WIFI, true)) return false;
    bool has = prefs.isKey("ssid") && prefs.getString("ssid", "").length() > 0;
    prefs.end();
    return has;
}

bool WiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setAutoConnect(true);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);  // Lower power for stability
    LOG_INFO(TAG, "WiFi initialized in STA mode");
    return true;
}

bool WiFiManager::autoConnect() {
    if (!hasWiFiConfig()) {
        LOG_INFO(TAG, "No WiFi config found, starting AP mode");
        startAPMode();
        return false;
    }
    return loadAndConnect();
}

bool WiFiManager::loadAndConnect() {
    if (!_prefs.begin(NVS_NAMESPACE_WIFI, true)) {
        LOG_ERROR(TAG, "Failed to open WiFi preferences");
        return false;
    }

    String ssid = _prefs.getString("ssid", "");
    String pass = _prefs.getString("pass", "");
    _prefs.end();

    if (ssid.length() == 0) {
        LOG_ERROR(TAG, "Empty SSID in NVS");
        return false;
    }

    return connect(ssid.c_str(), pass.c_str());
}

bool WiFiManager::connect(const char* ssid, const char* pass) {
    LOG_INFO(TAG, "Connecting to: %s", ssid);

    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    // Wait for connection (30s timeout)
    int retries = 0;
    const int MAX_RETRIES = 60;  // 60 * 500ms = 30s

    while (WiFi.status() != WL_CONNECTED && retries < MAX_RETRIES) {
        delay(500);
        retries++;

        if (retries % 10 == 0) {
            LOG_INFO(TAG, "Still connecting... (%d/%d)", retries, MAX_RETRIES);
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        LOG_INFO(TAG, "Connected! IP: %s RSSI: %d",
                 WiFi.localIP().toString().c_str(), WiFi.RSSI());

        if (_connectedCb) _connectedCb();
        return true;
    }

    LOG_ERROR(TAG, "Connection failed after %d retries", retries);
    return false;
}

void WiFiManager::startAPMode() {
    _apMode = true;
    _apSSID = generateAPSSID();

    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP);

    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);

    WiFi.softAPConfig(apIP, gateway, subnet);
    WiFi.softAP(_apSSID.c_str());

    // Start DNS for captive portal
    _dnsServer.start(53, "*", apIP);

    LOG_INFO(TAG, "AP Mode started: %s (IP: %s)",
             _apSSID.c_str(), WiFi.softAPIP().toString().c_str());
}

void WiFiManager::stopAPMode() {
    _dnsServer.stop();
    WiFi.softAPdisconnect(true);
    _apMode = false;
    LOG_INFO(TAG, "AP Mode stopped");
}

void WiFiManager::loop() {
    if (_apMode) {
        _dnsServer.processNextRequest();
    }
}

bool WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

WiFiStatus WiFiManager::getStatus() const {
    if (_apMode) return WiFiStatus::AP_MODE;
    switch (WiFi.status()) {
        case WL_CONNECTED:     return WiFiStatus::CONNECTED;
        case WL_IDLE_STATUS:   return WiFiStatus::CONNECTING;
        case WL_DISCONNECTED:  return WiFiStatus::DISCONNECTED;
        case WL_CONNECT_FAILED: return WiFiStatus::CONNECT_FAILED;
        default:               return WiFiStatus::UNKNOWN;
    }
}

String WiFiManager::getIP() const {
    if (_apMode) return WiFi.softAPIP().toString();
    return WiFi.localIP().toString();
}

String WiFiManager::getMAC() const { return WiFi.macAddress(); }
String WiFiManager::getSSID() const { return WiFi.SSID(); }
int WiFiManager::getRSSI() const { return WiFi.RSSI(); }

bool WiFiManager::saveCredentials(const char* ssid, const char* pass) {
    if (!_prefs.begin(NVS_NAMESPACE_WIFI, false)) {
        LOG_ERROR(TAG, "Failed to open WiFi NVS for writing");
        return false;
    }

    _prefs.putString("ssid", ssid);
    _prefs.putString("pass", pass ? pass : "");
    _prefs.end();

    LOG_INFO(TAG, "WiFi credentials saved for: %s", ssid);
    return true;
}

void WiFiManager::clearCredentials() {
    if (_prefs.begin(NVS_NAMESPACE_WIFI, false)) {
        _prefs.clear();
        _prefs.end();
    }
    LOG_INFO(TAG, "WiFi credentials cleared");
}

String WiFiManager::generateAPSSID() {
    String mac = WiFi.macAddress();
    String suffix = mac.substring(mac.length() - 5);
    suffix.replace(":", "");
    return String(AP_SSID_PREFIX) + suffix;
}
