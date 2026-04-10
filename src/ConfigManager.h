#pragma once
#include <Arduino.h>
#include <Preferences.h>

class ConfigManager {
public:
    static ConfigManager* getInstance();

    bool loadConfig();
    bool saveConfig();
    void resetToDefaults();

    // LED
    String getLedBrightness() const { return _ledBrightness; }
    void setLedBrightness(const String& val) { _ledBrightness = val; }

    // Buzzer
    bool getBuzzerEnabled() const { return _buzzerEnabled; }
    void setBuzzerEnabled(bool val) { _buzzerEnabled = val; }
    String getBuzzerVolume() const { return _buzzerVolume; }
    void setBuzzerVolume(const String& val) { _buzzerVolume = val; }

    // Permission
    int getPermissionTimeout() const { return _permissionTimeout; }
    void setPermissionTimeout(int val) { _permissionTimeout = val; }
    String getPermissionDefault() const { return _permissionDefault; }
    void setPermissionDefault(const String& val) { _permissionDefault = val; }

    // DND
    bool getDndMode() const { return _dndMode; }
    void setDndMode(bool val) { _dndMode = val; }

    // Sleep
    int getSleepTimeout() const { return _sleepTimeout; }
    void setSleepTimeout(int val) { _sleepTimeout = val; }

    // Time
    int getTimezone() const { return _timezone; }
    void setTimezone(int val) { _timezone = val; }
    String getNtpServer() const { return _ntpServer; }
    void setNtpServer(const String& val) { _ntpServer = val; }

    // mDNS
    String getMdnsHostname() const { return _mdnsHostname; }
    void setMdnsHostname(const String& val) { _mdnsHostname = val; }

private:
    ConfigManager();
    static ConfigManager* _instance;
    Preferences _prefs;

    String _ledBrightness;
    bool _buzzerEnabled;
    String _buzzerVolume;
    int _permissionTimeout;
    String _permissionDefault;
    bool _dndMode;
    int _sleepTimeout;
    int _timezone;
    String _ntpServer;
    String _mdnsHostname;
};
