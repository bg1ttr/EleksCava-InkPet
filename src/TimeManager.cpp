#include "TimeManager.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <time.h>

static const char* TAG = "Time";
TimeManager* TimeManager::_instance = nullptr;

TimeManager::TimeManager() : _synced(false), _lastSync(0), _gmtOffset(0) {}

TimeManager* TimeManager::getInstance() {
    if (!_instance) _instance = new TimeManager();
    return _instance;
}

void TimeManager::begin() {
    _gmtOffset = ConfigManager::getInstance()->getTimezone() * 3600;
    String ntpServer = ConfigManager::getInstance()->getNtpServer();

    configTime(_gmtOffset, 0, ntpServer.c_str(),
               "time.cloudflare.com", "ntp.aliyun.com");

    LOG_INFO(TAG, "NTP configured: server=%s, tz=%d",
             ntpServer.c_str(), ConfigManager::getInstance()->getTimezone());
}

bool TimeManager::syncTime(int timeoutSec) {
    LOG_INFO(TAG, "Syncing time (timeout: %ds)...", timeoutSec);

    struct tm timeinfo;
    int attempts = 0;

    while (attempts < timeoutSec) {
        if (getLocalTime(&timeinfo, 1000)) {
            if (timeinfo.tm_year > 100) {  // Year > 2000
                _synced = true;
                _lastSync = millis();
                LOG_INFO(TAG, "Time synced: %04d-%02d-%02d %02d:%02d:%02d",
                         timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                return true;
            }
        }
        attempts++;
    }

    LOG_WARNING(TAG, "Time sync failed after %d attempts", timeoutSec);
    return false;
}

String TimeManager::getFormattedTime() const {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return "??:??";

    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    return String(buf);
}

String TimeManager::getFormattedDate() const {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return "----/--/--";

    static const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                    "Jul","Aug","Sep","Oct","Nov","Dec"};
    static const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

    char buf[32];
    snprintf(buf, sizeof(buf), "%s, %s %d %d",
             days[timeinfo.tm_wday], months[timeinfo.tm_mon],
             timeinfo.tm_mday, timeinfo.tm_year + 1900);
    return String(buf);
}

int TimeManager::getCurrentHour() const {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) return -1;
    return timeinfo.tm_hour;
}

bool TimeManager::isTimeValid() const { return _synced; }

void TimeManager::update() {
    // Re-sync every hour
    if (_synced && millis() - _lastSync > 3600000) {
        syncTime(5);
    }
}
