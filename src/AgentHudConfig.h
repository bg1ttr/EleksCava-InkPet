#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>

struct AgentHudConfigData {
    bool enabled = true;
    bool showTaskTitle = true;
    bool showUsage = true;
    bool showProgress = true;
    bool showMultiTask = true;
    bool privacyHidePrompt = true;
    bool ledProgress = true;
    uint8_t maxVisibleTasks = 2;
    String displayDensity = "compact";
    String summaryMode = "single";
    String defaultView = "priority";
    String enabledAgents = "claude,codex,gemini,cursor,augment,vscode";
};

class AgentHudConfig {
public:
    static AgentHudConfig* getInstance();

    void begin();
    const AgentHudConfigData& data() const { return _data; }
    AgentHudConfigData& data() { return _data; }

    bool updateFromJson(JsonObject obj, bool persist);
    void writeJson(JsonObject obj) const;
    String toJson() const;
    bool save();
    void resetDefaults(bool persist);

private:
    AgentHudConfig();
    static AgentHudConfig* _instance;

    Preferences _prefs;
    AgentHudConfigData _data;
    bool _loaded;

    void load();
};
