#include "NVSManager.h"
#include "Logger.h"
#include <nvs_flash.h>

static const char* TAG = "NVS";
NVSManager* NVSManager::_instance = nullptr;

NVSManager* NVSManager::getInstance() {
    if (!_instance) _instance = new NVSManager();
    return _instance;
}

bool NVSManager::initialize() {
    if (_initialized) return true;

    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        LOG_WARNING(TAG, "NVS init error (%s), erasing and reinitializing...", esp_err_to_name(err));
        nvs_flash_erase();
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        LOG_ERROR(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return false;
    }

    _initialized = true;
    LOG_INFO(TAG, "NVS initialized successfully");

    // Check if first boot - use read-write mode since namespace may not exist
    Preferences prefs;
    if (prefs.begin("inkspet", false)) {
        if (!prefs.isKey("initialized")) {
            prefs.end();
            writeDefaultConfiguration();
        } else {
            prefs.end();
        }
    } else {
        // Namespace creation failed, force write defaults
        LOG_WARNING(TAG, "Cannot open inkspet namespace, writing defaults");
        writeDefaultConfiguration();
    }

    return true;
}

void NVSManager::resetToDefaults() {
    LOG_WARNING(TAG, "Resetting all configuration to defaults");
    Preferences prefs;
    if (prefs.begin("inkspet", false)) {
        prefs.clear();
        prefs.end();
    }
    if (prefs.begin("wifi", false)) {
        prefs.clear();
        prefs.end();
    }
    writeDefaultConfiguration();
}

void NVSManager::writeDefaultConfiguration() {
    LOG_INFO(TAG, "Writing default configuration");
    Preferences prefs;
    if (prefs.begin("inkspet", false)) {
        prefs.putBool("initialized", true);
        prefs.putString("ledBright", "medium");
        prefs.putBool("buzzerOn", false);
        prefs.putString("buzzerVol", "low");
        prefs.putInt("permTimeout", 60);
        prefs.putString("permDefault", "deny");
        prefs.putBool("dndMode", false);
        prefs.putInt("sleepTimeout", 60);
        prefs.putInt("tz", 8);
        prefs.putString("ntpSrv", "pool.ntp.org");
        prefs.putString("mdnsHost", "inkspet");
        prefs.end();
    }
}
