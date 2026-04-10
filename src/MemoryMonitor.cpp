#include "MemoryMonitor.h"
#include "Logger.h"
#include "config.h"

static const char* TAG = "Memory";
MemoryMonitor* MemoryMonitor::_instance = nullptr;

MemoryMonitor::MemoryMonitor()
    : _peakFree(0), _minFree(UINT32_MAX), _lastLog(0) {}

MemoryMonitor* MemoryMonitor::getInstance() {
    if (!_instance) _instance = new MemoryMonitor();
    return _instance;
}

void MemoryMonitor::update() {
    size_t free = ESP.getFreeHeap();
    if (free > _peakFree) _peakFree = free;
    if (free < _minFree) _minFree = free;

    if (millis() - _lastLog > 30000) {
        _lastLog = millis();
        LOG_INFO(TAG, "Heap: %u free, %u largest block, %.1f%% frag",
                 free, ESP.getMaxAllocHeap(), getFragmentation() * 100.0f);

        if (free < HEAP_CRITICAL) {
            LOG_ERROR(TAG, "CRITICAL: Heap below %d bytes!", HEAP_CRITICAL);
        } else if (free < HEAP_WARNING) {
            LOG_WARNING(TAG, "Low heap: %u bytes", free);
        }
    }
}

bool MemoryMonitor::isBelowCritical() const {
    return ESP.getFreeHeap() < HEAP_CRITICAL;
}

bool MemoryMonitor::hasEnoughForWebServer() const {
    return ESP.getFreeHeap() >= HEAP_MIN_WEBSERVER &&
           ESP.getMaxAllocHeap() >= HEAP_MIN_SSL;
}

size_t MemoryMonitor::getFreeHeap() const { return ESP.getFreeHeap(); }
size_t MemoryMonitor::getLargestBlock() const { return ESP.getMaxAllocHeap(); }

float MemoryMonitor::getFragmentation() const {
    size_t free = ESP.getFreeHeap();
    size_t largest = ESP.getMaxAllocHeap();
    if (free == 0) return 1.0f;
    return 1.0f - ((float)largest / (float)free);
}

String MemoryMonitor::getReport() const {
    char buf[128];
    snprintf(buf, sizeof(buf), "Free: %uB | Block: %uB | Frag: %.0f%% | Min: %uB",
             getFreeHeap(), getLargestBlock(), getFragmentation() * 100.0f, _minFree);
    return String(buf);
}
