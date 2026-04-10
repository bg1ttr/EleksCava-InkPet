#pragma once
#include <Arduino.h>

class MemoryMonitor {
public:
    static MemoryMonitor* getInstance();

    void update();
    bool isBelowCritical() const;
    bool hasEnoughForWebServer() const;
    size_t getFreeHeap() const;
    size_t getLargestBlock() const;
    float getFragmentation() const;
    String getReport() const;

private:
    MemoryMonitor();
    static MemoryMonitor* _instance;

    size_t _peakFree;
    size_t _minFree;
    unsigned long _lastLog;
};
