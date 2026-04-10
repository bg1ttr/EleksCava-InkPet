#pragma once
#include <Arduino.h>

class BatteryManager {
public:
    static BatteryManager* getInstance();
    void init();
    void update();

    float getVoltage() const { return _voltage; }
    int getPercentage() const { return _percentage; }
    bool isCharging() const { return _usbConnected; }
    bool isLow() const { return _percentage < 15; }

private:
    BatteryManager();
    static BatteryManager* _instance;

    float _voltage;
    int _percentage;
    bool _usbConnected;
    unsigned long _lastRead;

    float readVoltage();
    int voltageToPercent(float v);
};
