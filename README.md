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

- **Claude Desktop BLE (v1.1.0+)** — Native support for Anthropic's [Hardware Buddy](https://github.com/anthropics/claude-desktop-buddy) protocol. Pair over LE Secure Connections, receive live session snapshots, approve tool calls with physical buttons, receive GIF character packs — see [Claude Desktop Integration](#claude-desktop-integration-v110) below
- **12 Agent States** — Pixel art Clawd crab with unique pose per state
- **RGB LED Status** — Blue=thinking, Green=working, Red=error, Yellow=permission
- **Physical Permission Buttons** — Press A/B/C to approve/deny, zero window switching
- **Chinese / UTF-8 Text** — HUD and permission screen render Chinese tool names, file paths, and Claude transcript summaries (wqy12 font, ~3000 glyphs)
- **Tool Call Statistics** — Live Read/Write/Edit/Bash counts + elapsed time + file path
- **One-Click Hook Setup** — Web dashboard with copy-paste prompt, AI configures itself
- **DND Mode** — Long-press B to mute LED + buzzer, auto-deny permissions
- **Captive Portal** — AP mode auto-opens WiFi config page on phone
- **Web Dashboard** — Real-time status, device config, hook setup at `http://inkspet.local`
- **Multi-Agent Support** — Track up to 8 concurrent AI agent sessions with priority resolution
- **Dual-Channel** — HTTP webhooks (Claude Code / Copilot / Codex…) and Claude Desktop BLE run side-by-side with source-tagged permission routing
- **Open Source** — MIT license, full firmware + web portal

## Hardware

Based on the [EleksCava](https://elekscava.com) e-paper smart device:

- **MCU**: ESP32 (dual-core 240MHz, 320KB SRAM, **WiFi + Bluetooth LE 4.2**)
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
AI Coding Agents (HTTP)                 EleksCava Hardware                      Claude Desktop (BLE)
+------------------+                +---------------------------+             +---------------------+
| Claude Code      |---hook POST--->|                           |<---BLE------|  Hardware Buddy     |
| Copilot CLI      |---hook POST--->|   ESP32 WebServer :80     |  NUS        |  (Developer Mode)   |
| Codex CLI        |---hook POST--->|        +                  |             |                     |
| Gemini CLI       |---hook POST--->|   NimBLE NUS Server       |             |  snapshot / turn /  |
| Cursor Agent     |---hook POST--->|        +                  |             |  permission / file  |
| Kiro CLI         |---hook POST--->|   AgentStateManager       |             |  folder push        |
| opencode         |---hook POST--->|        +                  |             +---------------------+
+------------------+                |   Buddy HUD renderer      |
                                    |  +--------+--------+---+  |
                                    |  | E-Paper | LED  |Btn |  |
                                    |  +--------+--------+---+  |
                                    +---------------------------+
```

### Firmware Modules

| Module | Description |
|--------|-------------|
| `DisplayManager` | E-paper driver (GxEPD2), sidebar layout, Buddy HUD, CJK font auto-switch, anti-ghosting |
| `RGBLed` | WS2812 LED effects (solid, breathing, flash, rainbow, fade) |
| `AgentStateManager` | State machine, 12 states, priority resolution, multi-session tracking |
| `PermissionManager` | Permission request queue (max 4), source-tagged (BLE/HTTP), timeout auto-deny |
| `PixelArt` | 11 Clawd crab XBM bitmaps (48x48, 1-bit) |
| `WiFiManager` | WiFi STA connection, AP mode, credential storage (NVS) |
| `InksPetWebServer` | AsyncWebServer, REST API, WebSocket, LittleFS static files |
| `buddy/BleBridge` | NimBLE Nordic UART Service, LE Secure Connections, passkey bonding |
| `buddy/BuddyProtocol` | Hardware Buddy JSON line parser, snapshot / turn / cmd / status ack |
| `buddy/BuddyStateMapper` | Maps official 7-state BLE semantics to InksPet's 12-state enum |
| `buddy/FileXfer` | Folder-push receiver, base64 chunks → LittleFS, hot-reload GIF renderer |
| `buddy/BuddyStats` | NVS-backed approvals / denials / velocity / level / owner / pet name |
| `buddy/EpaperGifRenderer` | AnimatedGIF decoder → 1-bit Bayer-8×8 dithered framebuffer |
| `KeyManager` | Button debounce (50ms), long press, combo detection (A+C) |
| `BuzzerManager` | Passive buzzer melodies (permission alert, error, complete) |
| `BatteryManager` | ADC voltage reading, percentage calculation, USB detection |
| `TimeManager` | NTP sync (multi-server fallback), timezone support, BLE epoch sync |
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

## Claude Desktop Integration (v1.1.0+)

InksPet implements Anthropic's [Hardware Buddy BLE protocol](https://github.com/anthropics/claude-desktop-buddy/blob/main/REFERENCE.md) natively — pair it with the Claude desktop app over Bluetooth LE and it becomes a first-class approval panel and live session display, no HTTP hooks needed.

### Pairing

1. Claude desktop app → **Help → Troubleshooting → Enable Developer Mode**
2. **Developer → Open Hardware Buddy…** → **Connect** → pick `Claude-XXXX` from the list
   (the suffix is the last 2 bytes of your device's Bluetooth MAC)
3. InksPet shows a 6-digit passkey on the e-paper → type it into macOS when prompted
4. The bond persists across reboots; subsequent sessions reconnect automatically

### Supported capabilities

| Direction | Capability |
|-----------|------------|
| Desktop → Device | Heartbeat snapshot (total / running / waiting / msg / tokens / tokens_today / entries) |
| Desktop → Device | Permission prompts (`id` / `tool` / `hint`) |
| Desktop → Device | Turn events (SDK content array, text + tool calls) |
| Desktop → Device | Time sync (epoch + timezone offset) |
| Desktop → Device | Owner name, pet name |
| Desktop → Device | Folder push — drag a GIF character pack onto the Hardware Buddy window, streamed over BLE, written to LittleFS, hot-swapped into the renderer |
| Device → Desktop | Permission decision `{cmd:permission, decision:once \| deny}` — physical button A approves, C denies |
| Device → Desktop | Status ack (battery / heap / uptime / approvals / denials / velocity / level / bonded-flag) |
| Device → Desktop | `unpair` — erases the stored LE bond when Claude's "Forget" is pressed |

### Buddy HUD on the e-paper

```
┌──────────────────────────────────────────┐
│ ■■■■■ │ approve: Bash                    │  ← upstream msg (Chinese also supported)
│ [pix  │ Today: 31.2K                     │  ← tokens_today counter
│  art] │ · · · · · · · · · · · · · · · ·  │
│       │ · 10:42 git push                 │  ← recent transcript entries (≤3)
│ Work  │ · 10:41 yarn test                │
│       │ · 10:39 reading file...          │
├──────────────────────────────────────────┤
│ Claude-EA06                       bonded │  ← device name · link encryption
└──────────────────────────────────────────┘
```

### GIF character packs

Drag a folder containing `manifest.json` + per-state `.gif` files onto the Hardware Buddy window. The pack streams over BLE (base64 chunks with per-chunk acks, max ~1.8 MB), lands in `/characters/<name>/` on LittleFS, and the renderer switches to animated mode. Colour GIFs are dithered to 1-bit with an 8×8 Bayer matrix — monochrome panel, colour-original character.

See the [`bufo` example](https://github.com/anthropics/claude-desktop-buddy/tree/main/characters/bufo) in the upstream repo.

### Security

- **LE Secure Connections + MITM bonding** — NUS read/write is encryption-gated
- **DisplayOnly IO capability** — the 6-digit passkey is generated per-boot on the device side, typed on the macOS prompt (resists passive BLE sniffing)
- **`{cmd:unpair}` handling** — Claude's "Forget" button wipes the bond on the device too

### Works alongside HTTP webhooks

BLE and HTTP coexist without conflict. Claude Code hooks (HTTP) and Claude Desktop (BLE) can both drive InksPet at the same time — the AgentStateManager merges sessions by priority. Permission requests carry a **Source tag** (BLE vs HTTP) so button decisions always reply on the correct transport, no cross-channel leakage.

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

**v1.1.0 — Claude Desktop BLE integration live.** All v1.0.x features plus native Hardware Buddy protocol: BLE pairing over LE Secure Connections, live session HUD, physical-button tool approval replied back to Claude desktop, GIF character pack playback with 1-bit dithering, Chinese/UTF-8 rendering on the HUD. See [CHANGELOG.md](CHANGELOG.md).

## License

MIT
