#pragma once
#include <Arduino.h>
#include <Preferences.h>

class NVSManager {
public:
    static NVSManager* getInstance();
    bool initialize();
    void resetToDefaults();
    bool isAvailable() const { return _initialized; }

private:
    NVSManager() : _initialized(false) {}
    static NVSManager* _instance;
    bool _initialized;

    void writeDefaultConfiguration();
};
