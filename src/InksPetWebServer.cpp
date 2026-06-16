#include "InksPetWebServer.h"
#include "Logger.h"
#include "MemoryMonitor.h"
#include "ConfigManager.h"
#include "AgentStateManager.h"
#include "AgentHudConfig.h"
#include "PermissionManager.h"
#include "WiFiManager.h"
#include "BatteryManager.h"
#include "RGBLed.h"
#include "BuzzerManager.h"
#include "version.h"
#include "config.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

static const char* TAG = "WebServer";
InksPetWebServer* InksPetWebServer::_instance = nullptr;

InksPetWebServer::InksPetWebServer()
    : _server(nullptr), _ws(nullptr), _initialized(false) {}

InksPetWebServer::~InksPetWebServer() {
    end();
}

InksPetWebServer* InksPetWebServer::getInstance() {
    if (!_instance) _instance = new InksPetWebServer();
    return _instance;
}

bool InksPetWebServer::begin() {
    if (_initialized) return true;

    if (!MemoryMonitor::getInstance()->hasEnoughForWebServer()) {
        LOG_ERROR(TAG, "Insufficient memory for WebServer");
        return false;
    }

    _server = new(std::nothrow) AsyncWebServer(WEBSERVER_PORT);
    if (!_server) {
        LOG_ERROR(TAG, "Failed to create AsyncWebServer");
        return false;
    }

    // CORS headers
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

    setupAgentApiRoutes();
    setupConfigApiRoutes();
    setupWiFiApiRoutes();
    setupWebSocket();
    setupStaticFiles();

    // 404 handler
    _server->onNotFound([](AsyncWebServerRequest* request) {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else if (request->url().startsWith("/api/")) {
            request->send(404, "application/json", "{\"error\":\"endpoint not found\"}");
        } else {
            request->send(404, "text/plain", "Not found");
        }
    });

    _server->begin();
    _initialized = true;
    LOG_INFO(TAG, "WebServer started on port %d", WEBSERVER_PORT);
    return true;
}

void InksPetWebServer::end() {
    if (_server) {
        _server->end();
        delete _server;
        _server = nullptr;
    }
    if (_ws) {
        delete _ws;
        _ws = nullptr;
    }
    _initialized = false;
}

// ---- Agent API Routes ----
void InksPetWebServer::setupAgentApiRoutes() {
    // POST /api/agent/state - Receive agent state events
    _server->on("/api/agent/state", HTTP_POST,
        [](AsyncWebServerRequest* request) { request->send(400); },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            DynamicJsonDocument doc(4096);
            DeserializationError err = deserializeJson(doc, (const char*)data, len);

            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }

            JsonObject obj = doc.as<JsonObject>();
            String eventType = obj["event"] | "";

            AgentHudConfig::getInstance()->begin();
            if (!AgentHudConfig::getInstance()->data().enabled) {
                request->send(202, "application/json", "{\"success\":true,\"ignored\":\"agent hud disabled\"}");
                return;
            }

            // Handle permission requests specially
            if (eventType == "PermissionRequest") {
                PermissionManager::getInstance()->queueRequest(
                    obj["session"] | obj["agent"].as<String>(),
                    obj["agent"] | "unknown",
                    obj["tool"] | "",
                    obj["file"] | ""
                );
            }

            AgentStateManager::getInstance()->processEvent(obj);

            StaticJsonDocument<128> resp;
            resp["success"] = true;
            resp["state"] = AgentStateManager::stateToString(
                AgentStateManager::getInstance()->getCurrentState());

            String respStr;
            serializeJson(resp, respStr);
            request->send(200, "application/json", respStr);
        }
    );

    // POST /api/agent/permission/response - Permission response
    _server->on("/api/agent/permission/response", HTTP_POST,
        [](AsyncWebServerRequest* request) { request->send(400); },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<256> doc;
            DeserializationError err = deserializeJson(doc, (const char*)data, len);

            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }

            String sessionId = doc["session"] | "";
            String action = doc["action"] | "deny";

            bool cleared = PermissionManager::getInstance()->respondToSession(sessionId, action);
            if (!cleared) {
                AgentStateManager::getInstance()->respondToPermission(sessionId, action);
            }

            StaticJsonDocument<96> resp;
            resp["success"] = true;
            resp["cleared"] = cleared;
            String respStr;
            serializeJson(resp, respStr);
            request->send(200, "application/json", respStr);
        }
    );

    // GET /api/agent/status - Query current state
    _server->on("/api/agent/status", HTTP_GET, [](AsyncWebServerRequest* request) {
        auto* asm_ = AgentStateManager::getInstance();
        const AgentSession* session = asm_->getCurrentSession();

        StaticJsonDocument<2048> doc;
        doc["state"] = AgentStateManager::stateToString(asm_->getCurrentState());
        doc["active_sessions"] = asm_->getActiveSessionCount();
        doc["uptime"] = millis() / 1000;
        String localIp = WiFiManager::getInstance()->getIP();
        doc["localIP"] = localIp;
        doc["baseUrl"] = String("http://") + localIp;
        JsonObject config = doc.createNestedObject("config");
        AgentHudConfig::getInstance()->begin();
        AgentHudConfig::getInstance()->writeJson(config);

        if (session) {
            doc["agent"] = session->agentName;
            doc["session"] = session->sessionId;
            doc["tool"] = session->tool;
            doc["file"] = session->file;
            doc["priority"] = session->priority;

            JsonObject task = doc.createNestedObject("task");
            task["title"] = session->taskTitle;
            task["summary"] = session->taskSummary;
            task["promptSnippet"] = session->promptSnippet;
            task["repo"] = session->repo;
            task["branch"] = session->branch;
            task["action"] = session->currentAction;

            JsonObject usage = doc.createNestedObject("usage");
            JsonArray windows = usage.createNestedArray("windows");
            if (session->usageLine1.length()) windows.add(session->usageLine1);
            if (session->usageLine2.length()) windows.add(session->usageLine2);

            JsonObject progress = doc.createNestedObject("progress");
            progress["reliable"] = session->hasReliableProgress;
            progress["done"] = session->progressDone;
            progress["total"] = session->progressTotal;

            if (session->hasTasks) {
                JsonObject tasks = doc.createNestedObject("tasks");
                tasks["done"] = session->tasksDone;
                tasks["running"] = session->tasksRunning;
                tasks["pending"] = session->tasksPending;
                JsonArray items = tasks.createNestedArray("items");
                for (uint8_t i = 0; i < session->taskItemCount; i++) {
                    items.add(session->taskItems[i]);
                }
            }

            if (session->recentCompletionTitle.length()) {
                JsonObject recent = doc.createNestedObject("recentCompletion");
                recent["title"] = session->recentCompletionTitle;
                recent["ago"] = session->recentCompletionAgo;
            }

            if (session->questionOptionCount > 0) {
                JsonArray options = doc.createNestedArray("options");
                for (uint8_t i = 0; i < session->questionOptionCount; i++) {
                    options.add(session->questionOptions[i]);
                }
            }
        }

        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });

    // POST /api/agent/tasks - Update task progress from TodoWrite hook
    _server->on("/api/agent/tasks", HTTP_POST,
        [](AsyncWebServerRequest* request) { request->send(400); },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            DynamicJsonDocument doc(2048);
            DeserializationError err = deserializeJson(doc, (const char*)data, len);
            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            String session = doc["session"] | doc["agent"].as<String>();
            uint16_t done = doc["done"] | 0;
            uint16_t running = doc["running"] | 0;
            uint16_t pending = doc["pending"] | 0;
            String items[AgentSession::MAX_TASK_ITEMS];
            uint8_t itemCount = 0;
            JsonArray arr = doc["items"].as<JsonArray>();
            if (!arr.isNull()) {
                for (JsonVariant item : arr) {
                    String text;
                    if (item.is<const char*>()) {
                        text = item.as<String>();
                    } else {
                        JsonObject obj = item.as<JsonObject>();
                        if (!obj.isNull()) {
                            const char* keys[] = {"title", "content", "text", "summary", "action"};
                            for (const char* key : keys) {
                                if (!obj.containsKey(key)) continue;
                                text = obj[key].as<String>();
                                text.trim();
                                if (text.length()) break;
                            }
                        }
                    }
                    text.trim();
                    if (!text.length()) continue;
                    items[itemCount++] = text;
                    if (itemCount >= AgentSession::MAX_TASK_ITEMS) break;
                }
            }
            AgentStateManager::getInstance()->updateTasks(session, done, running, pending, items, itemCount);
            request->send(200, "application/json", "{\"success\":true}");
        }
    );

    // GET /api/agent/bridge.js - local bridge used by hook install scripts.
    _server->on("/api/agent/bridge.js", HTTP_GET, [](AsyncWebServerRequest* request) {
        const char* bridge =
R"JS(#!/usr/bin/env node
const http=require("http"),https=require("https"),os=require("os"),path=require("path"),fs=require("fs"),{URL}=require("url");
const HOME=process.env.INKSPET_AGENT_HOME||path.join(os.homedir(),".inkspet-agent-hud");
const CACHE=path.join(HOME,"last-completion.json");
const INDEX=path.join(os.homedir(),".codex","session_index.jsonl");
function args(a){const r={};for(let i=0;i<a.length;i++){if(!a[i].startsWith("--"))continue;const k=a[i].slice(2),n=a[i+1];if(!n||n.startsWith("--"))r[k]=true;else{r[k]=n;i++;}}return r;}
function stdin(){return new Promise(ok=>{let d="";process.stdin.setEncoding("utf8");process.stdin.on("data",c=>d+=c);process.stdin.on("end",()=>ok(d.trim()));});}
function first(){for(const v of arguments)if(typeof v==="string"&&v.trim())return v.trim();return "";}
function nested(o){let v=o;for(let i=1;i<arguments.length;i++){if(!v||typeof v!=="object")return"";v=v[arguments[i]];}return typeof v==="string"?v.trim():"";}
function gitRoot(d){try{d=fs.realpathSync(d)}catch(_){}while(d&&d!==path.dirname(d)){if(fs.existsSync(path.join(d,".git")))return d;d=path.dirname(d)}return process.cwd();}
function repoName(input,a){const cwd=first(a.cwd,input.cwd,nested(input,"workspace","current_dir"),nested(input,"workspace","cwd"),nested(input,"project","cwd"),process.cwd());return path.basename(gitRoot(cwd));}
function sid(s,agent){s=String(s||"").trim();for(const p of [agent+"-","codex-","claude-","cursor-","gemini-","augment-","vscode-"])if(s.startsWith(p))return s.slice(p.length);return s;}
function codexTitle(session,agent){const id=sid(session,agent);if(!id)return"";let raw="";try{raw=fs.readFileSync(INDEX,"utf8")}catch(_){return"";}const lines=raw.split(/\r?\n/);for(let i=lines.length-1;i>=0;i--){const line=lines[i].trim();if(!line)continue;try{const e=JSON.parse(line);if(e&&e.id===id&&typeof e.thread_name==="string")return e.thread_name.trim();}catch(_){}}return"";}
function latestCodexTitle(){let raw="";try{raw=fs.readFileSync(INDEX,"utf8")}catch(_){return"";}const lines=raw.split(/\r?\n/);for(let i=lines.length-1;i>=0;i--){const line=lines[i].trim();if(!line)continue;try{const e=JSON.parse(line);if(e&&typeof e.thread_name==="string")return e.thread_name.trim();}catch(_){}}return"";}
function looksCmd(v){const t=String(v||"").trim();return/^(git|node|npm|pnpm|yarn|python3?|bash|sh|zsh|curl|pio|make|cmake|cargo|go|rsync|mkdir|rm|cp|mv|apply_patch|sed|rg|grep|cat|ls|find)\b/.test(t)||/^\$ /.test(t);}
function validTitle(v,blocked){const t=String(v||"").trim();if(!t||looksCmd(t))return"";const l=t.toLowerCase();for(const b of blocked){const x=String(b||"").trim();if(x&&l===x.toLowerCase())return"";}return t;}
function readCache(){try{return JSON.parse(fs.readFileSync(CACHE,"utf8"));}catch(_){return null;}}
function writeCache(e){try{fs.mkdirSync(path.dirname(CACHE),{recursive:true});fs.writeFileSync(CACHE,JSON.stringify(e));}catch(_){}}
function ago(ms){if(!Number.isFinite(ms)||ms<0)return"";const s=Math.floor(ms/1000);if(s<60)return s+"s ago";const m=Math.floor(s/60);if(m<60)return m+"m ago";const h=Math.floor(m/60);if(h<24)return h+"h ago";return Math.floor(h/24)+"d ago";}
function titleOf(o){const blocked=[o.repo,o.action,o.tool,o.ti.command,o.ti.file_path,o.ti.path,o.input.command],direct=validTitle(first(o.a.title,o.input.conversation_title,o.input.thread_name,o.input.session_title),blocked);if(direct)return direct;const ci=validTitle(codexTitle(o.session,o.agent),blocked);if(ci)return ci;if(o.agent==="codex"){const li=validTitle(latestCodexTitle(),blocked);if(li)return li;}return validTitle(first(o.task.title,o.input.title),blocked)||"Waiting for next step";}
function state(ev,explicit){if(explicit)return explicit;ev=String(ev||"");if(ev==="PermissionRequest")return"permission";if(ev==="Ask"||ev==="Question"||ev==="UserInputRequest")return"ask";if(ev==="PostToolUseFailure"||ev==="Error")return"error";if(ev==="UserPromptSubmit"||ev==="SessionStart")return"thinking";if(ev==="Stop"||ev==="PostCompact"||ev==="Completed")return"completed";if(ev==="PreToolUse"||ev==="PostToolUse"||ev==="StateUpdate")return"working";return"idle";}
function todos(ti){const a=ti&&Array.isArray(ti.todos)?ti.todos:null;if(!a||!a.length)return undefined;const r={done:0,running:0,pending:0,items:[]},c=[];for(const t of a){if(!t||typeof t!=="object")continue;const s=String(t.status||"").toLowerCase(),x=first(t.content,t.title,t.text,t.summary);if(s==="completed")r.done++;else if(s==="in_progress"||s==="running"){r.running++;if(x)c.push(x);}else{r.pending++;if(x)c.push(x);}}r.items=c.slice(0,3);return r;}
function post(base,ep,payload){return new Promise((ok,fail)=>{const u=new URL(ep,base),b=JSON.stringify(payload),c=u.protocol==="https:"?https:http;const req=c.request({hostname:u.hostname,port:u.port||(u.protocol==="https:"?443:80),path:u.pathname+u.search,method:"POST",headers:{"Content-Type":"application/json","Content-Length":Buffer.byteLength(b)},timeout:5000,insecureHTTPParser:true},res=>{let d="";res.setEncoding("utf8");res.on("data",x=>d+=x);res.on("end",()=>res.statusCode>=200&&res.statusCode<300?ok(d?JSON.parse(d):{}):fail(new Error("HTTP "+res.statusCode+": "+d)));});req.on("timeout",()=>req.destroy(new Error("request timeout")));req.on("error",fail);req.write(b);req.end();});}
function norm(input,a){const agent=first(a.agent,input.agent,input.source,"codex"),ev=first(a.event,input.event,input.hook_event_name,input.type,"StateUpdate"),session=first(a.session,input.session,input.session_id,input.conversation_id,agent+"-"+os.hostname()+"-"+path.basename(process.cwd())),task=input.task&&typeof input.task==="object"?input.task:{},ti=input.tool_input&&typeof input.tool_input==="object"?input.tool_input:{},ts=todos(ti),tool=first(a.tool,input.tool,input.tool_name),repo=first(a.repo,task.repo,input.repo,repoName(input,a)),action=first(a.action,task.action,task.current,input.action,ti.command,ti.file_path,ti.path,tool),title=titleOf({a,input,task,session,agent,repo,action,tool,ti});let recent;const c=readCache();if(c&&c.title)recent={title:c.title,ago:ago(Date.now()-(Number(c.ts)||0))};if((ev==="Stop"||ev==="PostCompact"||ev==="Completed")&&title!=="Waiting for next step"){writeCache({title,ts:Date.now()});recent={title,ago:"just now"};}return{agent,session,event:ev,state:state(ev,first(a.state,input.state)),priority:Number(a.priority||input.priority||80),tool,file:first(a.file,input.file,input.path,input.command,ti.file_path,ti.path,ti.command),task:{title,summary:first(a.summary,task.summary,input.summary,input.message),promptSnippet:first(a.prompt,task.promptSnippet,input.promptSnippet,input.prompt),repo,branch:first(a.branch,task.branch,input.branch,process.env.GIT_BRANCH),action},recentCompletion:recent,permission:first(a.permissionCommand,input.permissionCommand,ti.command,ti.file_path,ti.path)?{command:first(a.permissionCommand,input.permissionCommand,ti.command,ti.file_path,ti.path)}:undefined,tasks:ts,question:input.question,options:input.options,usage:input.usage,progress:input.progress||(ts?{reliable:true,done:ts.done,total:ts.done+ts.running+ts.pending}:{reliable:false,done:0,total:0}),display:{language:first(a.language,input.language,"en-US")}};}
(async()=>{const a=args(process.argv.slice(2)),raw=await stdin();let input={};if(raw){try{input=JSON.parse(raw)}catch{input={prompt:raw}}}const payload=norm(input,a),base=a.device||process.env.INKSPET_AGENT_URL||"http://inkspet.local",ep=a.endpoint||"/api/agent/state";const result=await post(base,ep,payload);if(payload.tasks)await post(base,"/api/agent/tasks",Object.assign({session:payload.session,agent:payload.agent},payload.tasks));console.log(JSON.stringify({ok:true,payload,result},null,2));})().catch(e=>{console.error("[inkspet-agent] "+e.message);process.exit(1);});
)JS";
        AsyncWebServerResponse* response = request->beginResponse(200, "application/javascript", bridge);
        response->addHeader("Cache-Control", "no-cache");
        request->send(response);
    });

    // GET /api/agent/install.sh - macOS/Linux hook helper.
    _server->on("/api/agent/install.sh", HTTP_GET, [](AsyncWebServerRequest* request) {
        String host = request->host();
        if (!host.length()) host = WiFiManager::getInstance()->getIP();
        String baseUrl = String("http://") + host;
        String agent = "codex";
        if (request->hasParam("agent")) {
            agent = request->getParam("agent")->value();
            agent.toLowerCase();
        }

        String script;
        script.reserve(5200);
        script += "#!/bin/sh\nset -eu\n";
        script += "DEVICE_URL=\"";
        script += baseUrl;
        script += "\"\n";
        script += "AGENT=\"${INKSPET_AGENT:-";
        script += agent;
        script += "}\"\n";
        script += "DIR=\"${INKSPET_AGENT_HOME:-$HOME/.inkspet-agent-hud}\"\nmkdir -p \"$DIR\"\n";
        script += "curl -fsSL --connect-timeout 5 \"$DEVICE_URL/api/agent/bridge.js\" -o \"$DIR/inkspet-agent-bridge.js\"\n";
        script += "chmod +x \"$DIR/inkspet-agent-bridge.js\"\n";
        script += "cat > \"$DIR/hook-config.json\" <<JSON\n";
        script += "{\"device\":{\"baseUrl\":\"";
        script += baseUrl;
        script += "\",\"stateEndpoint\":\"/api/agent/state\",\"tasksEndpoint\":\"/api/agent/tasks\",\"permissionEndpoint\":\"/api/agent/permission/response\"},\"supportedAgents\":[\"codex\",\"claude\",\"gemini\",\"cursor\",\"augment\",\"vscode\"],\"bridgeCommand\":\"INKSPET_AGENT_URL=";
        script += baseUrl;
        script += " node $DIR/inkspet-agent-bridge.js\"}\nJSON\n";
        script += "cat > \"$DIR/codex-hook-command.txt\" <<EOF\n";
        script += "INKSPET_AGENT_URL=";
        script += baseUrl;
        script += " node $DIR/inkspet-agent-bridge.js --agent codex\nEOF\n";
        script += "cat > \"$DIR/claude-code-hook-command.txt\" <<EOF\n";
        script += "INKSPET_AGENT_URL=";
        script += baseUrl;
        script += " node $DIR/inkspet-agent-bridge.js --agent claude\nEOF\n";
        script += "cat > \"$DIR/install-codex-hook.js\" <<'NODE'\n";
        script += "const fs=require('fs'),os=require('os'),path=require('path');\n";
        script += "const home=os.homedir(),dir=process.env.DIR,device=process.env.DEVICE_URL;\n";
        script += "const hooksPath=path.join(home,'.codex','hooks.json');\n";
        script += "const bridge=path.join(dir,'inkspet-agent-bridge.js');\n";
        script += "const cmd=`INKSPET_AGENT_URL=${device} node ${bridge} --agent codex`;\n";
        script += "const events=['PermissionRequest','PreToolUse','PostToolUse','PostToolUseFailure','SessionStart','Stop','UserPromptSubmit'];\n";
        script += "fs.mkdirSync(path.dirname(hooksPath),{recursive:true});\n";
        script += "let data={hooks:{}};\n";
        script += "if(fs.existsSync(hooksPath)){const raw=fs.readFileSync(hooksPath,'utf8').trim();if(raw)data=JSON.parse(raw);const stamp=new Date().toISOString().replace(/[:.]/g,'-');fs.copyFileSync(hooksPath,`${hooksPath}.backup.inkspet-${stamp}`);}\n";
        script += "if(!data.hooks||typeof data.hooks!=='object')data.hooks={};\n";
        script += "for(const ev of events){\n";
        script += "  if(!Array.isArray(data.hooks[ev])||!data.hooks[ev].length)data.hooks[ev]=[{hooks:[]}];\n";
        script += "  for(const group of data.hooks[ev])if(!Array.isArray(group.hooks))group.hooks=[];\n";
        script += "  let kept=false;\n";
        script += "  for(const group of data.hooks[ev]){\n";
        script += "    group.hooks=group.hooks.filter(h=>{const isInk=String(h.command||'').includes('inkspet-agent-bridge.js');if(!isInk)return true;if(!kept){kept=true;h.type='command';h.command=cmd;h.timeout=ev==='PermissionRequest'?10:5;return true;}return false;});\n";
        script += "  }\n";
        script += "  if(!kept)data.hooks[ev][0].hooks.push({type:'command',command:cmd,timeout:ev==='PermissionRequest'?10:5});\n";
        script += "}\n";
        script += "fs.writeFileSync(hooksPath,JSON.stringify(data,null,2)+'\\n');\n";
        script += "console.log('Codex hook updated without removing existing hooks: '+hooksPath);\n";
        script += "NODE\n";
        script += "if [ \"$AGENT\" = \"codex\" ] || [ \"$AGENT\" = \"all\" ]; then DEVICE_URL=\"$DEVICE_URL\" DIR=\"$DIR\" node \"$DIR/install-codex-hook.js\"; fi\n";
        script += "node \"$DIR/inkspet-agent-bridge.js\" --device \"$DEVICE_URL\" --agent \"$AGENT\" --event UserPromptSubmit --session inkspet-install-test --title \"InksPet install test\" --summary \"Bridge installed and thinking state sent.\" </dev/null\n";
        script += "echo \"InksPet AI HUD bridge installed in $DIR\"\n";
        script += "echo \"Copy commands from $DIR/*-hook-command.txt for tools that do not use Codex hooks.\"\n";

        AsyncWebServerResponse* response = request->beginResponse(200, "text/x-shellscript", script);
        response->addHeader("Content-Disposition", "attachment; filename=\"inkspet-agent-install.sh\"");
        response->addHeader("Cache-Control", "no-cache");
        request->send(response);
    });

    // POST /api/agent/config/test
    _server->on("/api/agent/config/test", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            StaticJsonDocument<256> doc;
            AgentHudConfig::getInstance()->begin();
            doc["success"] = true;
            doc["endpoint"] = "/api/agent/state";
            doc["hudEnabled"] = AgentHudConfig::getInstance()->data().enabled;
            doc["activeSessions"] = AgentStateManager::getInstance()->getActiveSessionCount();
            doc["state"] = AgentStateManager::stateToString(AgentStateManager::getInstance()->getCurrentState());
            doc["schema"] = "agent/session/event/state/task/usage/progress/display/priority";
            String resp;
            serializeJson(doc, resp);
            request->send(200, "application/json", resp);
        },
        nullptr,
        [](AsyncWebServerRequest*, uint8_t*, size_t, size_t, size_t) {}
    );

    // GET /api/agent/config
    _server->on("/api/agent/config", HTTP_GET, [](AsyncWebServerRequest* request) {
        AgentHudConfig::getInstance()->begin();
        request->send(200, "application/json", AgentHudConfig::getInstance()->toJson());
    });

    // POST /api/agent/config
    _server->on("/api/agent/config", HTTP_POST,
        [](AsyncWebServerRequest* request) { request->send(400); },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<1024> doc;
            DeserializationError err = deserializeJson(doc, (const char*)data, len);
            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }
            bool ok = AgentHudConfig::getInstance()->updateFromJson(doc.as<JsonObject>(), true);
            request->send(ok ? 200 : 500, "application/json",
                          ok ? "{\"success\":true}" : "{\"error\":\"save failed\"}");
        }
    );

    // GET /api/device/info - Device discovery
    _server->on("/api/device/info", HTTP_GET, [](AsyncWebServerRequest* request) {
        StaticJsonDocument<512> doc;
        doc["name"] = FIRMWARE_NAME;
        doc["version"] = VERSION;
        doc["hardware"] = HARDWARE_PLATFORM;
        doc["ip"] = WiFiManager::getInstance()->getIP();
        doc["mac"] = WiFiManager::getInstance()->getMAC();
        doc["heap"] = ESP.getFreeHeap();
        doc["uptime"] = millis() / 1000;

        auto* batt = BatteryManager::getInstance();
        doc["battery_voltage"] = batt->getVoltage();
        doc["battery_percent"] = batt->getPercentage();
        doc["charging"] = batt->isCharging();

        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });
}

// ---- Config API Routes ----
void InksPetWebServer::setupConfigApiRoutes() {
    // GET /api/config - Get all config
    _server->on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
        auto* cfg = ConfigManager::getInstance();
        StaticJsonDocument<512> doc;

        doc["led_brightness"] = cfg->getLedBrightness();
        doc["buzzer_enabled"] = cfg->getBuzzerEnabled();
        doc["buzzer_volume"] = cfg->getBuzzerVolume();
        doc["permission_timeout"] = cfg->getPermissionTimeout();
        doc["permission_default"] = cfg->getPermissionDefault();
        doc["dnd_mode"] = cfg->getDndMode();
        doc["sleep_timeout"] = cfg->getSleepTimeout();
        doc["timezone"] = cfg->getTimezone();
        doc["ntp_server"] = cfg->getNtpServer();
        doc["mdns_hostname"] = cfg->getMdnsHostname();

        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });

    // POST /api/config - Update config
    _server->on("/api/config", HTTP_POST,
        [](AsyncWebServerRequest* request) { request->send(400); },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<512> doc;
            DeserializationError err = deserializeJson(doc, (const char*)data, len);

            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }

            auto* cfg = ConfigManager::getInstance();

            if (doc.containsKey("led_brightness")) cfg->setLedBrightness(doc["led_brightness"]);
            if (doc.containsKey("buzzer_enabled")) cfg->setBuzzerEnabled(doc["buzzer_enabled"]);
            if (doc.containsKey("buzzer_volume")) cfg->setBuzzerVolume(doc["buzzer_volume"]);
            if (doc.containsKey("permission_timeout")) cfg->setPermissionTimeout(doc["permission_timeout"]);
            if (doc.containsKey("permission_default")) cfg->setPermissionDefault(doc["permission_default"]);
            if (doc.containsKey("dnd_mode")) cfg->setDndMode(doc["dnd_mode"]);
            if (doc.containsKey("sleep_timeout")) cfg->setSleepTimeout(doc["sleep_timeout"]);
            if (doc.containsKey("timezone")) cfg->setTimezone(doc["timezone"]);
            if (doc.containsKey("ntp_server")) cfg->setNtpServer(doc["ntp_server"]);
            if (doc.containsKey("mdns_hostname")) cfg->setMdnsHostname(doc["mdns_hostname"]);

            cfg->saveConfig();
            RGBLed::getInstance()->setBrightnessLevel(cfg->getLedBrightness());
            BuzzerManager::getInstance()->setEnabled(cfg->getBuzzerEnabled());
            BuzzerManager::getInstance()->setVolumeLevel(cfg->getBuzzerVolume());
            request->send(200, "application/json", "{\"success\":true}");
        }
    );

    // POST /api/config/reset - Factory reset
    _server->on("/api/config/reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        ConfigManager::getInstance()->resetToDefaults();
        AgentHudConfig::getInstance()->resetDefaults(true);
        WiFiManager::getInstance()->clearCredentials();  // Clear WiFi credentials so device boots into AP mode
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Config reset to defaults\"}");
        delay(1000);
        ESP.restart();
    });
}

// ---- WiFi API Routes ----
void InksPetWebServer::setupWiFiApiRoutes() {
    // POST /api/wifi/scan - Scan networks
    _server->on("/api/wifi/scan", HTTP_POST, [](AsyncWebServerRequest* request) {
        WiFi.scanNetworks(true);  // Async scan
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Scanning...\"}");
    });

    // GET /api/wifi/scan - Get scan results
    _server->on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest* request) {
        int n = WiFi.scanComplete();

        if (n == WIFI_SCAN_RUNNING) {
            request->send(200, "application/json", "{\"scanning\":true}");
            return;
        }

        DynamicJsonDocument doc(2048);
        doc["scanning"] = false;
        JsonArray networks = doc.createNestedArray("networks");

        if (n > 0) {
            for (int i = 0; i < n && i < 20; i++) {
                JsonObject net = networks.createNestedObject();
                net["ssid"] = WiFi.SSID(i);
                net["rssi"] = WiFi.RSSI(i);
                net["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
            }
        }

        WiFi.scanDelete();

        String resp;
        serializeJson(doc, resp);
        request->send(200, "application/json", resp);
    });

    // POST /api/wifi/connect - Connect to network
    _server->on("/api/wifi/connect", HTTP_POST,
        [](AsyncWebServerRequest* request) { request->send(400); },
        nullptr,
        [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t, size_t) {
            StaticJsonDocument<256> doc;
            DeserializationError err = deserializeJson(doc, (const char*)data, len);

            if (err) {
                request->send(400, "application/json", "{\"error\":\"invalid json\"}");
                return;
            }

            String ssid = doc["ssid"] | "";
            String pass = doc["password"] | "";

            if (ssid.length() == 0) {
                request->send(400, "application/json", "{\"error\":\"SSID required\"}");
                return;
            }

            WiFiManager::getInstance()->saveCredentials(ssid.c_str(), pass.c_str());
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Credentials saved. Restarting...\"}");

            // Restart to apply new WiFi
            delay(1000);
            ESP.restart();
        }
    );

    // Compatibility endpoints
    _server->on("/ip", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", WiFiManager::getInstance()->getIP());
    });

    _server->on("/mac", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/plain", WiFiManager::getInstance()->getMAC());
    });
}

// ---- WebSocket ----
void InksPetWebServer::setupWebSocket() {
    _ws = new AsyncWebSocket("/ws");

    _ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                         AwsEventType type, void* arg, uint8_t* data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                LOG_INFO(TAG, "WebSocket client #%u connected", client->id());
                sendDeviceInfo(client);
                break;
            case WS_EVT_DISCONNECT:
                LOG_INFO(TAG, "WebSocket client #%u disconnected", client->id());
                break;
            case WS_EVT_DATA: {
                AwsFrameInfo* info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                    char* msg = new char[len + 1];
                    memcpy(msg, data, len);
                    msg[len] = '\0';
                    handleWebSocketMessage(client, msg);
                    delete[] msg;
                }
                break;
            }
            default:
                break;
        }
    });

    _server->addHandler(_ws);
}

void InksPetWebServer::handleWebSocketMessage(AsyncWebSocketClient* client, const char* data) {
    StaticJsonDocument<200> doc;
    DeserializationError err = deserializeJson(doc, data);
    if (err) return;

    const char* type = doc["type"];
    if (!type) return;

    if (strcmp(type, "ping") == 0) {
        client->text("{\"type\":\"pong\"}");
    } else if (strcmp(type, "requestDeviceInfo") == 0) {
        sendDeviceInfo(client);
    }
}

void InksPetWebServer::sendDeviceInfo(AsyncWebSocketClient* client) {
    StaticJsonDocument<512> doc;
    doc["type"] = "deviceInfo";
    doc["ip"] = WiFiManager::getInstance()->getIP();
    doc["mac"] = WiFiManager::getInstance()->getMAC();
    doc["ssid"] = WiFiManager::getInstance()->getSSID();
    doc["rssi"] = WiFiManager::getInstance()->getRSSI();
    doc["heap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;
    doc["version"] = VERSION;
    doc["state"] = AgentStateManager::stateToString(
        AgentStateManager::getInstance()->getCurrentState());
    doc["active_sessions"] = AgentStateManager::getInstance()->getActiveSessionCount();

    String resp;
    serializeJson(doc, resp);
    client->text(resp);
}

void InksPetWebServer::broadcastState() {
    if (!_ws || _ws->count() == 0) return;

    StaticJsonDocument<1024> doc;
    doc["type"] = "stateUpdate";
    doc["state"] = AgentStateManager::stateToString(
        AgentStateManager::getInstance()->getCurrentState());
    doc["active_sessions"] = AgentStateManager::getInstance()->getActiveSessionCount();

    const AgentSession* session = AgentStateManager::getInstance()->getCurrentSession();
    if (session) {
        doc["agent"] = session->agentName;
        doc["session"] = session->sessionId;
        doc["tool"] = session->tool;
        doc["file"] = session->file;
        JsonObject task = doc.createNestedObject("task");
        task["title"] = session->taskTitle;
        task["repo"] = session->repo;
        task["branch"] = session->branch;
        task["action"] = session->currentAction;
        JsonObject progress = doc.createNestedObject("progress");
        progress["reliable"] = session->hasReliableProgress;
        progress["done"] = session->progressDone;
        progress["total"] = session->progressTotal;
        if (session->hasTasks) {
            JsonObject tasks = doc.createNestedObject("tasks");
            tasks["done"] = session->tasksDone;
            tasks["running"] = session->tasksRunning;
            tasks["pending"] = session->tasksPending;
        }
    }

    String resp;
    serializeJson(doc, resp);
    _ws->textAll(resp);
}

// ---- Static Files ----
void InksPetWebServer::setupStaticFiles() {
    if (!LittleFS.begin()) {
        LOG_ERROR(TAG, "Failed to mount LittleFS");
        return;
    }

    // Serve config page
    _server->on("/config", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!LittleFS.exists("/config.html")) {
            request->send(404, "text/plain", "Config page not found");
            return;
        }
        request->send(LittleFS, "/config.html", "text/html");
    });

    // Serve static files from LittleFS
    _server->serveStatic("/", LittleFS, "/")
        .setDefaultFile("index.html")
        .setCacheControl("max-age=600");

    LOG_INFO(TAG, "Static files configured from LittleFS");
}
