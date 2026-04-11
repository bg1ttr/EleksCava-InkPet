# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.


## Overview

InksPet 是一个基于 EleksCava 硬件平台的 AI 编程助手桌面状态监视器。ESP32 + 2.9" 电子墨水屏 + RGB LED + 物理按键，通过 HTTP Webhook 接收 AI Agent（Claude Code、Copilot 等）的状态事件，在墨水屏上显示像素画角色，用 LED 颜色指示状态，物理按键审批权限请求。

## Tech Stack

- **MCU**: ESP32-WROOM-32 (Arduino framework, PlatformIO)
- **Display**: 2.9" e-paper (296x128, SSD1680) via GxEPD2 + U8g2
- **LED**: 5x WS2812B via Adafruit NeoPixel (GPIO 16, GRB order)
- **Network**: AsyncWebServer (port 80) + mDNS
- **Storage**: LittleFS for assets, NVS for config
- **Buzzer**: Passive, PWM on GPIO 25

## Build Commands

```bash
pio run                  # Build firmware
pio run -t upload        # Build and flash
pio run -t buildfs       # Build LittleFS filesystem image
pio run -t uploadfs      # Upload filesystem to device
pio run -t clean         # Clean build artifacts
pio device monitor       # Serial monitor (115200 baud)
```

## Architecture

The firmware is structured as modular components:

| Module | Status | Role |
|--------|--------|------|
| `WiFiManager` | Reuse from EleksCava | WiFi connection + AP config portal |
| `DisplayManager` | Reuse from EleksCava | E-paper driver, fonts, refresh |
| `RGBLed` | Reuse from EleksCava | WS2812 LED effects (breathing, flash) |
| `WebServer` | Reuse framework | HTTP API for agent hooks |
| `AgentStateManager` | **New** | State machine, multi-agent priority |
| `PixelArtRenderer` | **New** | 12-state pixel art rendering |
| `PermissionManager` | **New** | Permission queue + button approval |


## API Endpoints

- `POST /api/agent/state` — Receive agent state events (thinking, typing, error, etc.)
- `POST /api/agent/permission/response` — Return permission decisions (allow/always_allow/deny)
- `GET /api/agent/status` — Query current device state

12 agent states with priority order:
`PermissionRequest > Error > Working > Thinking > Conducting > Juggling > Sweeping > Carrying > Completed > Idle > Sleeping`

## Hardware Constraints (Must Know)

- **RAM ~320KB, no PSRAM**: Only one SSL connection at a time; free heap must stay >80KB (critical: 40KB)
- **E-paper refresh**: Partial ~300ms (800ms cooldown), full ~2s (3000ms cooldown). Force full refresh every 30 partials. Use mutex for display ops.
- **Pixel art**: 12 states x 48x48 x 1-bit = ~3.5KB total, store in PROGMEM or LittleFS
- **Boot stability**: Reduce LED brightness during boot; 500ms delay before WiFi init; use 80MHz CPU during WiFi init

## Key GPIO Pins

```
E-paper SPI: SCK=18, MOSI=23, CS=26, DC=27, RST=14, BUSY=12
Buttons: A=33, B=13, C=5 (INPUT_PULLUP, active LOW, 50ms debounce)
RGB LED: 16 (5x WS2812B)
Buzzer: 25 (PWM)
Battery ADC: 32 (divider 2.21:1)
```

## Development Milestones

| Phase | Goal |
|-------|------|
| M1 | WiFi + WebServer + LED status |
| M2 | Pixel art + e-paper state display |
| M3 | Permission approval + buttons + buzzer |
| M4 | Multi-agent session management |
| M5 | Web config portal |
| M6 | Statistics, custom art, widget mode |

## Key Docs

- `docs/PRD.md` — Full product requirements (Chinese)
- `docs/Hardware_Specification.md` — GPIO map, memory budget, display specs, build config

---

## 发版规则（提交变更前必读）

**任何对外发布的代码改动都需要完成以下三步，缺一不可。**

### 1. 同步更新版本号（两处）

| 文件 | 字段 |
|------|------|
| `src/version.h` | `#define FIRMWARE_VERSION "X.Y.Z"` |
| `platformio.ini` | `-DINKSPET_VERSION=\"X.Y.Z\"` |

> **为什么两处都要改**：`platformio.ini` 的 `-D` 编译标志优先级高于头文件，`version.h` 的 `#ifndef` 保护在此失效。只改 `version.h` 时，`/api/device/info` 返回的版本号仍是 `platformio.ini` 里的旧值。

版本号规则：
- `patch`（X.Y.**Z**）：仅 bug fix，对外行为不变
- `minor`（X.**Y**.0）：新显示功能、新 LED 效果、新 API
- `major`（**X**.0.0）：硬件兼容性变更、协议破坏性变更

### 2. Clean 构建并打包

```bash
pio run -t clean      # 必须 clean，确保版本号完整编译进固件
pio run               # 编译固件
pio run -t buildfs    # 编译 LittleFS 文件系统
python3 build.py      # 输出到 firmware/，生成带时间戳的 bin + manifest
```

### 3. 同步更新 inkspet-web（inkspet.com 固件发布页）

固件站点位于 `~/GitHub/inkspet-web`，完整 SOP 见该仓库的 `CLAUDE.md`。
简要步骤：

```bash
VERSION=X.Y.Z
cd ~/GitHub/inkspet-web
mkdir -p flash/v${VERSION}
cp ~/GitHub/EleksCava-InkPet/firmware/*v${VERSION}*.bin flash/v${VERSION}/
cp ~/GitHub/EleksCava-InkPet/firmware/manifest-v${VERSION}.json flash/v${VERSION}/
```

然后更新以下三个文件：
- `flash/manifest.json` — 指向新版本 bin 路径
- `flash/index.html` — 版本选择器 + 版本描述 + changelog 摘要卡片
- `changelog/index.html` — 顶部插入新版本条目，旧 latest 去掉 class

最后提交两个仓库：
```bash
# 固件仓库
git add firmware/ platformio.ini src/version.h CHANGELOG.md
git commit -m "fix/feat: <描述> — bump v${VERSION}"

# inkspet-web
cd ~/GitHub/inkspet-web
git add flash/ changelog/index.html
git commit -m "feat(firmware): add v${VERSION} — <描述>"
```
