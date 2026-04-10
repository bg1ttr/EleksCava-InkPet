#pragma once
#include <Arduino.h>

class TimeManager {
public:
    static TimeManager* getInstance();
    void begin();
    bool syncTime(int timeoutSec = 15);

    String getFormattedTime() const;
    String getFormattedDate() const;
    int getCurrentHour() const;
    bool isTimeValid() const;
    void update();

private:
    TimeManager();
    static TimeManager* _instance;

    bool _synced;
    unsigned long _lastSync;
    int _gmtOffset;
};
