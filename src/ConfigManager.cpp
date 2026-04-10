#include "ConfigManager.h"
#include "Logger.h"

static const char* TAG = "Config";
ConfigManager* ConfigManager::_instance = nullptr;

ConfigManager::ConfigManager()
    : _ledBrightness("medium"),
      _buzzerEnabled(false),
      _buzzerVolume("low"),
      _permissionTimeout(60),
      _permissionDefault("deny"),
      _dndMode(false),
      _sleepTimeout(60),
      _timezone(8),
      _ntpServer("pool.ntp.org"),
      _mdnsHostname("inkspet")
{}

ConfigManager* ConfigManager::getInstance() {
    if (!_instance) {
        _instance = new ConfigManager();
        _instance->loadConfig();
    }
    return _instance;
}

bool ConfigManager::loadConfig() {
    if (!_prefs.begin("inkspet", true)) {
        LOG_ERROR(TAG, "Failed to open NVS for reading");
        return false;
    }

    _ledBrightness = _prefs.getString("ledBright", "medium");
    _buzzerEnabled = _prefs.getBool("buzzerOn", true);
    _buzzerVolume = _prefs.getString("buzzerVol", "low");
    _permissionTimeout = _prefs.getInt("permTimeout", 60);
    _permissionDefault = _prefs.getString("permDefault", "deny");
    _dndMode = _prefs.getBool("dndMode", false);
    _sleepTimeout = _prefs.getInt("sleepTimeout", 60);
    _timezone = _prefs.getInt("tz", 8);
    _ntpServer = _prefs.getString("ntpSrv", "pool.ntp.org");
    _mdnsHostname = _prefs.getString("mdnsHost", "inkspet");

    _prefs.end();
    LOG_INFO(TAG, "Configuration loaded: LED=%s, Buzzer=%s, TZ=%d",
             _ledBrightness.c_str(), _buzzerEnabled ? "on" : "off", _timezone);
    return true;
}

bool ConfigManager::saveConfig() {
    if (!_prefs.begin("inkspet", false)) {
        LOG_ERROR(TAG, "Failed to open NVS for writing");
        return false;
    }

    _prefs.putString("ledBright", _ledBrightness);
    _prefs.putBool("buzzerOn", _buzzerEnabled);
    _prefs.putString("buzzerVol", _buzzerVolume);
    _prefs.putInt("permTimeout", _permissionTimeout);
    _prefs.putString("permDefault", _permissionDefault);
    _prefs.putBool("dndMode", _dndMode);
    _prefs.putInt("sleepTimeout", _sleepTimeout);
    _prefs.putInt("tz", _timezone);
    _prefs.putString("ntpSrv", _ntpServer);
    _prefs.putString("mdnsHost", _mdnsHostname);

    _prefs.end();
    LOG_INFO(TAG, "Configuration saved");
    return true;
}

void ConfigManager::resetToDefaults() {
    _ledBrightness = "medium";
    _buzzerEnabled = true;
    _buzzerVolume = "low";
    _permissionTimeout = 60;
    _permissionDefault = "deny";
    _dndMode = false;
    _sleepTimeout = 60;
    _timezone = 8;
    _ntpServer = "pool.ntp.org";
    _mdnsHostname = "inkspet";
    saveConfig();
    LOG_INFO(TAG, "Configuration reset to defaults");
}
