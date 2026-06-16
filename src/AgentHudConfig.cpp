#include "AgentHudConfig.h"
#include "Logger.h"

static const char* TAG = "AgentHudCfg";
AgentHudConfig* AgentHudConfig::_instance = nullptr;

static bool jsonBool(JsonObject obj, const char* key, bool fallback) {
    return obj.containsKey(key) ? (bool)obj[key] : fallback;
}

AgentHudConfig::AgentHudConfig() : _loaded(false) {}

AgentHudConfig* AgentHudConfig::getInstance() {
    if (!_instance) _instance = new AgentHudConfig();
    return _instance;
}

void AgentHudConfig::begin() {
    if (!_loaded) load();
}

void AgentHudConfig::load() {
    if (!_prefs.begin("agenthud", true)) {
        LOG_WARNING(TAG, "Failed to open NVS for reading");
        _loaded = true;
        return;
    }

    _data.enabled = _prefs.getBool("enabled", _data.enabled);
    _data.showTaskTitle = _prefs.getBool("taskTitle", _data.showTaskTitle);
    _data.showUsage = _prefs.getBool("usage", _data.showUsage);
    _data.showProgress = _prefs.getBool("progress", _data.showProgress);
    _data.showMultiTask = _prefs.getBool("multi", _data.showMultiTask);
    _data.privacyHidePrompt = _prefs.getBool("hidePrompt", _data.privacyHidePrompt);
    _data.ledProgress = _prefs.getBool("ledProg", _data.ledProgress);
    _data.maxVisibleTasks = _prefs.getUChar("maxTasks", _data.maxVisibleTasks);
    _data.displayDensity = _prefs.getString("density", _data.displayDensity);
    _data.summaryMode = _prefs.getString("summary", _data.summaryMode);
    _data.defaultView = _prefs.getString("defaultView", _data.defaultView);
    _data.enabledAgents = _prefs.getString("agents", _data.enabledAgents);
    _prefs.end();

    if (_data.maxVisibleTasks < 1 || _data.maxVisibleTasks > 3) {
        _data.maxVisibleTasks = 2;
    }
    if (!_data.displayDensity.length()) _data.displayDensity = "compact";
    if (!_data.summaryMode.length()) _data.summaryMode = "single";
    if (!_data.defaultView.length()) _data.defaultView = "priority";
    if (!_data.enabledAgents.length()) {
        _data.enabledAgents = "claude,codex,gemini,cursor,augment,vscode";
    }

    _loaded = true;
    LOG_INFO(TAG, "Loaded: enabled=%d density=%s ledProgress=%d",
             _data.enabled, _data.displayDensity.c_str(), _data.ledProgress);
}

bool AgentHudConfig::save() {
    if (!_prefs.begin("agenthud", false)) {
        LOG_ERROR(TAG, "Failed to open NVS for writing");
        return false;
    }

    bool ok = true;
    ok &= _prefs.putBool("enabled", _data.enabled) > 0;
    ok &= _prefs.putBool("taskTitle", _data.showTaskTitle) > 0;
    ok &= _prefs.putBool("usage", _data.showUsage) > 0;
    ok &= _prefs.putBool("progress", _data.showProgress) > 0;
    ok &= _prefs.putBool("multi", _data.showMultiTask) > 0;
    ok &= _prefs.putBool("hidePrompt", _data.privacyHidePrompt) > 0;
    ok &= _prefs.putBool("ledProg", _data.ledProgress) > 0;
    ok &= _prefs.putUChar("maxTasks", _data.maxVisibleTasks) > 0;
    ok &= _prefs.putString("density", _data.displayDensity) > 0;
    ok &= _prefs.putString("summary", _data.summaryMode) > 0;
    ok &= _prefs.putString("defaultView", _data.defaultView) > 0;
    ok &= _prefs.putString("agents", _data.enabledAgents) > 0;
    _prefs.end();

    LOG_INFO(TAG, "Saved: ok=%d", ok);
    return ok;
}

bool AgentHudConfig::updateFromJson(JsonObject obj, bool persist) {
    if (obj.isNull()) return false;
    begin();

    _data.enabled = jsonBool(obj, "enabled", _data.enabled);
    _data.defaultView = obj["defaultView"] | _data.defaultView;

    JsonObject display = obj["display"].as<JsonObject>();
    if (!display.isNull()) {
        _data.showTaskTitle = jsonBool(display, "taskTitle", _data.showTaskTitle);
        _data.showUsage = jsonBool(display, "usage", _data.showUsage);
        _data.showProgress = jsonBool(display, "progress", _data.showProgress);
        _data.showMultiTask = jsonBool(display, "multiTask", _data.showMultiTask);
        _data.displayDensity = display["density"] | _data.displayDensity;
        _data.summaryMode = display["summaryMode"] | _data.summaryMode;
    }

    JsonObject privacy = obj["privacy"].as<JsonObject>();
    if (!privacy.isNull()) {
        _data.privacyHidePrompt = jsonBool(privacy, "hidePrompt", _data.privacyHidePrompt);
    }

    JsonObject led = obj["led"].as<JsonObject>();
    if (!led.isNull()) {
        _data.ledProgress = jsonBool(led, "progress", _data.ledProgress);
    }

    JsonObject multiTask = obj["multiTask"].as<JsonObject>();
    if (!multiTask.isNull()) {
        uint8_t maxTasks = multiTask["maxVisible"] | _data.maxVisibleTasks;
        if (maxTasks < 1) maxTasks = 1;
        if (maxTasks > 3) maxTasks = 3;
        _data.maxVisibleTasks = maxTasks;
    }

    if (obj["agents"].is<const char*>()) {
        _data.enabledAgents = obj["agents"].as<const char*>();
    } else if (obj["agents"].is<JsonArray>()) {
        String joined;
        for (JsonVariant value : obj["agents"].as<JsonArray>()) {
            const char* agent = value.as<const char*>();
            if (!agent || !agent[0]) continue;
            if (joined.length()) joined += ",";
            joined += agent;
        }
        if (joined.length()) _data.enabledAgents = joined;
    }

    return persist ? save() : true;
}

void AgentHudConfig::writeJson(JsonObject obj) const {
    obj["enabled"] = _data.enabled;
    obj["defaultView"] = _data.defaultView;
    obj["enabledAgents"] = _data.enabledAgents;

    JsonObject display = obj["display"].to<JsonObject>();
    display["taskTitle"] = _data.showTaskTitle;
    display["usage"] = _data.showUsage;
    display["progress"] = _data.showProgress;
    display["multiTask"] = _data.showMultiTask;
    display["density"] = _data.displayDensity;
    display["summaryMode"] = _data.summaryMode;

    JsonObject privacy = obj["privacy"].to<JsonObject>();
    privacy["hidePrompt"] = _data.privacyHidePrompt;

    JsonObject led = obj["led"].to<JsonObject>();
    led["progress"] = _data.ledProgress;
    led["states"] = "blue thinking, green working, yellow permission, red error";

    JsonObject multiTask = obj["multiTask"].to<JsonObject>();
    multiTask["maxVisible"] = _data.maxVisibleTasks;
    multiTask["priority"] = "permission,error,working,thinking,completed,idle";
}

String AgentHudConfig::toJson() const {
    StaticJsonDocument<1024> doc;
    JsonObject obj = doc.to<JsonObject>();
    writeJson(obj);
    String out;
    serializeJson(doc, out);
    return out;
}

void AgentHudConfig::resetDefaults(bool persist) {
    _data = AgentHudConfigData();
    _loaded = true;
    if (persist) save();
}
