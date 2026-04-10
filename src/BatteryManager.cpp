#include "BatteryManager.h"
#include "config.h"
#include "Logger.h"

static const char* TAG = "Battery";
BatteryManager* BatteryManager::_instance = nullptr;

BatteryManager::BatteryManager()
    : _voltage(0), _percentage(0), _usbConnected(false), _lastRead(0)
{}

BatteryManager* BatteryManager::getInstance() {
    if (!_instance) _instance = new BatteryManager();
    return _instance;
}

void BatteryManager::init() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    pinMode(USB_STATUS_PIN, INPUT);
    update();
    LOG_INFO(TAG, "Battery monitor initialized: %.2fV (%d%%)", _voltage, _percentage);
}

void BatteryManager::update() {
    if (millis() - _lastRead < 5000) return;  // Read every 5s max
    _lastRead = millis();

    _voltage = readVoltage();
    _percentage = voltageToPercent(_voltage);
    _usbConnected = (digitalRead(USB_STATUS_PIN) == HIGH);
}

float BatteryManager::readVoltage() {
    // Multi-sample for stability
    uint32_t sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += analogRead(BATT_ADC_PIN);
        delayMicroseconds(2000);
    }
    float adcAvg = (float)sum / 5.0f;
    float voltage = (adcAvg / 4095.0f) * 3.3f * VOLTAGE_DIVIDER_RATIO;
    return voltage;
}

int BatteryManager::voltageToPercent(float v) {
    if (v >= BATT_MAX_VOLTAGE) return 100;
    if (v <= BATT_MIN_VOLTAGE) return 0;

    // Non-linear LiPo curve approximation
    float pct = (v - BATT_MIN_VOLTAGE) / (BATT_MAX_VOLTAGE - BATT_MIN_VOLTAGE);
    // Apply curve: more accurate for LiPo
    if (pct > 0.8f) pct = 0.8f + (pct - 0.8f) * 1.0f;
    else if (pct > 0.2f) pct = 0.2f + (pct - 0.2f) * 1.0f;
    else pct = pct * 1.0f;

    return constrain((int)(pct * 100.0f), 0, 100);
}
