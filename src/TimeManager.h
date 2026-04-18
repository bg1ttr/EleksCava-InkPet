#pragma once
#include <Arduino.h>

class TimeManager {
public:
    static TimeManager* getInstance();
    void begin();
    bool syncTime(int timeoutSec = 15);

    // Set the system clock directly from an epoch timestamp (seconds) plus a
    // timezone offset (seconds east of UTC). Used when the Claude desktop
    // buddy pushes a one-shot time sync over BLE.
    void setTimeFromEpoch(uint32_t epoch, int32_t tzOffsetSec);

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
