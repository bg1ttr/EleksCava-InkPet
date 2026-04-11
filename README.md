**English** | [中文](README.zh-CN.md) | [日本語](README.ja.md)

# InksPet by EleksCava

> Your AI coding agent, alive on e-paper. **EleksCava x Claude**

InksPet turns your [EleksCava](https://elekscava.com) e-paper device into a desktop AI agent monitor. Watch your Claude Code, Copilot, Codex, and other AI coding agents think, type, and build in real-time on a 2.9" e-ink display with RGB LED status light.

Website: [inkspet.com](https://inkspet.com) | Get Device: [elekscava.com](https://elekscava.com)

## Why InksPet + EleksCava?

InksPet is firmware. [EleksCava](https://elekscava.com) is the hardware. Together they turn a desktop e-paper device into an AI agent companion.

| Software Agent Monitor | InksPet on EleksCava |
|------------------------|---------------------|
| Takes up screen space | Dedicated device, zero screen space |
| Easy to miss behind windows | E-paper always visible, LED in peripheral vision |
| No physical feedback | RGB LED — sense status at a glance |
| Click to approve permissions | Physical buttons — no window switching |
| Another app on your machine | Low-power e-ink device, always on |

## Features

- **12 Agent States** — Pixel art Clawd crab with unique pose per state
- **RGB LED Status** — Blue=thinking, Green=working, Red=error, Yellow=permission
- **Physical Permission Buttons** — Press A/B/C to approve/deny, zero window switching
- **Tool Call Statistics** — Live Read/Write/Edit/Bash counts + elapsed time + file path
- **One-Click Hook Setup** — Web dashboard with copy-paste prompt, AI configures itself
- **DND Mode** — Long-press B to mute LED + buzzer, auto-deny permissions
- **Captive Portal** — AP mode auto-opens WiFi config page on phone
- **Web Dashboard** — Real-time status, device config, hook setup at `http://inkspet.local`
- **Multi-Agent Support** — Track up to 8 concurrent AI agent sessions with priority resolution
- **Open Source** — MIT license, full firmware + web portal

## Hardware

Based on the [EleksCava](https://elekscava.com) e-paper smart device:

- **MCU**: ESP32 (dual-core 240MHz, 320KB SRAM, WiFi)
- **Display**: 2.9" e-paper (296 x 128 px, black & white, always-on)
- **LED**: WS2812B RGB x5 (frosted acrylic diffuser)
- **Buttons**: 3 physical keys (A / B / C)
- **Buzzer**: Passive buzzer (default off, configurable)
- **Battery**: 3.7V Li-Po with USB-C charging
- **Power**: USB-C

## 12 Agent States

Rule of thumb: **Blue = thinking, Green = working, Red = error, Yellow = act**

| State | Pixel Art | LED Color | LED Effect | Trigger |
|-------|-----------|-----------|------------|---------|
| Sleeping | Curled up, zzz | Off | — | 60s+ idle |
| Idle | Relaxed pose | White dim | Solid | No activity |
| Thinking | Thought bubble | Blue | Breathing | `UserPromptSubmit` |
| Working | Keyboard pose | Green | Solid | `PreToolUse` / `PostToolUse` |
| Completed | Celebrating | Green | Fade once | `Stop` / `PostCompact` |
| Error | Shocked face, ! | Red | Fast flash | `PostToolUseFailure` |
| Permission | Question mark | Yellow | Flash | `PermissionRequest` |
| Juggling | Tossing ball | Purple | Breathing | `SubagentStart` (1-2 sessions) |
| Conducting | Baton raised | Purple | Solid | `SubagentStart` (3+ sessions) |
| Sweeping | Broom | Cyan | Breathing | `PreCompact` |
| Carrying | Holding box | Orange | Solid | `WorktreeCreate` |

## Permission Approval

When an AI agent requests tool permission, the e-paper displays:

```
+----------------------------------------------+
| ! PERMISSION REQUEST                         |
|                                              |
| Claude Code wants to:                        |
| [Edit  src/server.cpp                      ] |
|                                              |
| [A] ALLOW    [B] ALWAYS    [C] DENY         |
+----------------------------------------------+
     LED: yellow flashing
```

Press **A** to allow, **B** to always allow, or **C** to deny — no window switching needed.

## Architecture

```
AI Coding Agents                    EleksCava Hardware
+------------------+               +------------------+
| Claude Code      |--hook POST--->|                  |
| Copilot CLI      |--hook POST--->| ESP32 WebServer  |
| Codex CLI        |--hook POST--->|   port 80        |
| Gemini CLI       |--hook POST--->|                  |
| Cursor Agent     |--hook POST--->| AgentStateManager|
| Kiro CLI         |--hook POST--->|       |          |
| opencode         |--hook POST--->|       v          |
+------------------+               | +------+------+  |
                                   | |E-Paper|  LED|  |
                                   | +------+------+  |
                                   +------------------+
```

### Firmware Modules

| Module | Description |
|--------|-------------|
| `DisplayManager` | E-paper driver (GxEPD2), sidebar layout, anti-ghosting, font rendering |
| `RGBLed` | WS2812 LED effects (solid, breathing, flash, rainbow, fade) |
| `AgentStateManager` | State machine, 12 states, priority resolution, multi-session tracking |
| `PermissionManager` | Permission request queue (max 4), timeout auto-deny, button approval |
| `PixelArt` | 11 Clawd crab XBM bitmaps (48x48, 1-bit) |
| `WiFiManager` | WiFi STA connection, AP mode, credential storage (NVS) |
| `InksPetWebServer` | AsyncWebServer, REST API, WebSocket, LittleFS static files |
| `KeyManager` | Button debounce (50ms), long press, combo detection (A+C) |
| `BuzzerManager` | Passive buzzer melodies (permission alert, error, complete) |
| `BatteryManager` | ADC voltage reading, percentage calculation, USB detection |
| `TimeManager` | NTP sync (multi-server fallback), timezone support |
| `ConfigManager` | NVS persistent settings (LED, buzzer, permission, DND, timezone) |
| `MemoryMonitor` | Heap tracking, fragmentation alerts |
| `Logger` | Tagged serial logging (INFO/ERROR/WARNING/DEBUG) |

## API

### Agent State Endpoint

```
POST /api/agent/state
Content-Type: application/json
```

```json
{
  "agent": "claude-code",
  "session": "abc123",
  "event": "PreToolUse",
  "tool": "Edit",
  "file": "src/main.cpp"
}
```

### Supported Events

| Event | Mapped State |
|-------|-------------|
| `UserPromptSubmit` | thinking |
| `PreToolUse` | working |
| `PostToolUse` | working |
| `PostToolUseFailure` | error |
| `SubagentStart` (1-2 sessions) | juggling |
| `SubagentStart` (3+ sessions) | conducting |
| `Stop` / `PostCompact` | completed |
| `PreCompact` | sweeping |
| `WorktreeCreate` | carrying |
| `PermissionRequest` | permission |

### Permission Response Endpoint

```
POST /api/agent/permission/response
Content-Type: application/json
```

```json
{
  "session": "abc123",
  "action": "allow"
}
```

Actions: `allow`, `always_allow`, `deny`

### Status Query

```
GET /api/agent/status
```

```json
{
  "state": "working",
  "agent": "claude-code",
  "session": "abc123",
  "tool": "Edit",
  "file": "src/main.cpp",
  "uptime": 3600,
  "active_sessions": 2
}
```

### Device Info

```
GET /api/device/info
```

### Config

```
GET /api/config
POST /api/config
POST /api/config/reset
```

## Hook Setup

### The Easy Way

Open the InksPet Web Dashboard at `http://inkspet.local` (or device IP), click **Hook Setup**, copy the prompt, paste it to your AI coding assistant. It configures itself. 10 seconds.

### Manual Setup (Claude Code)

Add to your `~/.claude/settings.json` hooks section:

```json
{
  "hooks": {
    "UserPromptSubmit": [{ "hooks": [{
      "type": "command",
      "command": "curl -s -m 3 -X POST http://inkspet.local/api/agent/state -H 'Content-Type: application/json' -d '{\"agent\":\"claude-code\",\"event\":\"UserPromptSubmit\"}' 2>/dev/null || true",
      "timeout": 3
    }]}],
    "PreToolUse": [{ "matcher": "*", "hooks": [{
      "type": "command",
      "command": "jq -r '.tool_name // empty' | { read -r t; curl -s -m 3 -X POST http://inkspet.local/api/agent/state -H 'Content-Type: application/json' -d \"{\\\"agent\\\":\\\"claude-code\\\",\\\"event\\\":\\\"PreToolUse\\\",\\\"tool\\\":\\\"$t\\\"}\" 2>/dev/null || true; }",
      "timeout": 3
    }]}],
    "PostToolUse": [{ "matcher": "*", "hooks": [{
      "type": "command",
      "command": "curl -s -m 3 -X POST http://inkspet.local/api/agent/state -H 'Content-Type: application/json' -d '{\"agent\":\"claude-code\",\"event\":\"PostToolUse\"}' 2>/dev/null || true",
      "timeout": 3
    }]}],
    "Stop": [{ "hooks": [{
      "type": "command",
      "command": "curl -s -m 3 -X POST http://inkspet.local/api/agent/state -H 'Content-Type: application/json' -d '{\"agent\":\"claude-code\",\"event\":\"Stop\"}' 2>/dev/null || true",
      "timeout": 3
    }]}]
  }
}
```

Works with any AI tool that supports HTTP webhooks: Claude Code, Copilot, Cursor, Codex, Gemini CLI, OpenCode.

## Build

### Quick Flash (Browser)

Visit [inkspet.com/flash](https://inkspet.com/flash), connect device via USB-C, click Install.

### Build from Source

```bash
# Build firmware + filesystem
python3 build.py

# Or step by step
pio run                  # Build firmware
pio run -t buildfs       # Build LittleFS filesystem

# Flash to device (interactive menu: flash / log-only / backup / monitor)
./flash_all.sh /dev/cu.usbserial-XXXXX

# Jump straight into long-term monitor dashboard
./flash_all.sh /dev/cu.usbserial-XXXXX -m

# Use saved config from .flash_config
./flash_all.sh /dev/cu.usbserial-XXXXX -c

# Show help
./flash_all.sh -h
```

`flash_all.sh` provides four operation modes via interactive menu:

1. **Flash firmware / filesystem** — full flash or firmware-only, with optional erase
2. **Log device output** — serial capture to `logs/serial_*.log`
3. **Backup firmware** — dumps full 4MB flash to `backups/`
4. **Long-term monitor** — real-time dashboard tracking reboots, crashes, memory, watchdog, WiFi drops, agent events, permission requests, display refreshes, API success rate, and device stability (MTBF). Generates JSON stats and summary report on exit.

### Build Output

`python3 build.py` compiles firmware + filesystem, copies versioned binaries to `firmware/`, and generates `manifest.json` for ESP Web Tools.

## Tech Stack

- **MCU**: ESP32 (Arduino framework, PlatformIO)
- **Display**: GxEPD2 + U8g2 bitmap fonts
- **Network**: ESPAsyncWebServer + WebSocket + mDNS
- **LED**: Adafruit NeoPixel (WS2812B)
- **Storage**: LittleFS (web portal assets), NVS (configuration)
- **Web Portal**: Vanilla HTML/CSS/JS, dark theme, responsive

## Status

**v1.0.0 — Firmware functional.** Agent state display, permission approval, LED sync, tool statistics, Web dashboard with one-click hook setup all working. Next: enclosure design and small batch production.

## License

MIT
