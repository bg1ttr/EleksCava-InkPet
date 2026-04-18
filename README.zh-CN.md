[English](README.md) | **中文** | [日本語](README.ja.md)

# InksPet by EleksCava

> AI 编程助手，跃然墨水屏。**EleksCava x Claude**

InksPet 将你的 [EleksCava](https://elekscava.com) 墨水屏设备变成桌面 AI 智能体监视器。在 2.9 英寸电子墨水屏上实时观看 Claude Code、Copilot、Codex 等 AI 编程助手的思考、输入和构建过程，配合 RGB LED 状态指示灯。

官网: [inkspet.com](https://inkspet.com) | 购买设备: [elekscava.com](https://elekscava.com)

## 为什么选择 InksPet + EleksCava？

InksPet 是固件，[EleksCava](https://elekscava.com) 是硬件。两者结合，将桌面墨水屏设备变成 AI 智能体伴侣。

| 软件端智能体监视器 | InksPet on EleksCava |
|------------------------|---------------------|
| 占用屏幕空间 | 独立设备，零屏幕占用 |
| 容易被窗口遮挡 | 墨水屏常亮可见，LED 余光可感知 |
| 无物理反馈 | RGB LED -- 一眼感知状态 |
| 点击鼠标授权 | 物理按键 -- 无需切换窗口 |
| 只是电脑上的又一个应用 | 低功耗墨水屏设备，始终在线 |

## 功能特性

- **Claude 桌面 App 蓝牙 (v1.1.0+)** -- 原生支持 Anthropic 官方 [Hardware Buddy](https://github.com/anthropics/claude-desktop-buddy) 协议。通过 LE Secure Connections 配对，接收实时会话快照，物理按键审批工具请求，接收 GIF 字符包 -- 详见下方 [Claude 桌面 App 集成](#claude-桌面-app-集成-v110)
- **12 种智能体状态** -- 像素风 Clawd 螃蟹，每种状态独特姿态
- **RGB LED 状态灯** -- 蓝色=思考中，绿色=工作中，红色=错误，黄色=待授权
- **物理授权按键** -- 按 A/B/C 批准/拒绝，无需切换窗口
- **中文 / UTF-8 文本** -- HUD 与权限屏可显示中文工具名、文件路径、Claude 会话摘要 (wqy12 字体, ~3000 字形)
- **工具调用统计** -- 实时 Read/Write/Edit/Bash 计数 + 已用时间 + 文件路径
- **一键 Hook 配置** -- Web 面板提供复制粘贴提示词，AI 自行完成配置
- **免打扰模式** -- 长按 B 键静音 LED + 蜂鸣器，自动拒绝权限请求
- **强制门户** -- AP 模式下手机自动弹出 WiFi 配置页面
- **Web 控制面板** -- 实时状态、设备配置、Hook 配置，访问 `http://inkspet.local`
- **多智能体支持** -- 同时追踪最多 8 个 AI 智能体会话，优先级自动解析
- **双通道** -- HTTP webhooks (Claude Code / Copilot / Codex...) 与 Claude 桌面 BLE 并行工作，权限请求按来源标签路由回复
- **开源** -- MIT 协议，完整固件 + Web 门户

## 硬件

基于 [EleksCava](https://elekscava.com) 墨水屏智能设备:

- **MCU**: ESP32 (双核 240MHz, 320KB SRAM, **WiFi + 蓝牙 LE 4.2**)
- **显示屏**: 2.9 英寸墨水屏 (296 x 128 px, 黑白, 常亮)
- **LED**: WS2812B RGB x5 (磨砂亚克力扩散罩)
- **按键**: 3 个物理按键 (A / B / C)
- **蜂鸣器**: 无源蜂鸣器 (默认关闭, 可配置)
- **电池**: 3.7V 锂聚合物电池, USB-C 充电
- **供电**: USB-C

## 12 种智能体状态

经验法则: **蓝色 = 思考中, 绿色 = 工作中, 红色 = 出错了, 黄色 = 需操作**

| 状态 | 像素画 | LED 颜色 | LED 效果 | 触发条件 |
|-------|-----------|-----------|------------|---------|
| 睡眠中 | 蜷缩, zzz | 关闭 | -- | 空闲超过 60 秒 |
| 空闲 | 放松姿态 | 白色(暗) | 常亮 | 无活动 |
| 思考中 | 思考气泡 | 蓝色 | 呼吸 | `UserPromptSubmit` |
| 工作中 | 键盘姿态 | 绿色 | 常亮 | `PreToolUse` / `PostToolUse` |
| 已完成 | 庆祝 | 绿色 | 渐灭一次 | `Stop` / `PostCompact` |
| 出错 | 震惊脸, ! | 红色 | 快闪 | `PostToolUseFailure` |
| 待授权 | 问号 | 黄色 | 闪烁 | `PermissionRequest` |
| 分身中 | 抛球 | 紫色 | 呼吸 | `SubagentStart` (1-2 个会话) |
| 指挥中 | 举指挥棒 | 紫色 | 常亮 | `SubagentStart` (3+ 个会话) |
| 清理中 | 扫帚 | 青色 | 呼吸 | `PreCompact` |
| 搬运中 | 搬箱子 | 橙色 | 常亮 | `WorktreeCreate` |

## 权限审批

当 AI 智能体请求工具权限时，墨水屏显示:

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

按 **A** 允许, **B** 始终允许, 或 **C** 拒绝 -- 无需切换窗口。

## 系统架构

```
AI 编码智能体 (HTTP)                    EleksCava 硬件                         Claude 桌面 App (BLE)
+------------------+                +---------------------------+             +---------------------+
| Claude Code      |---hook POST--->|                           |<---BLE------|  Hardware Buddy     |
| Copilot CLI      |---hook POST--->|   ESP32 WebServer :80     |  NUS        |  (开发者模式)       |
| Codex CLI        |---hook POST--->|        +                  |             |                     |
| Gemini CLI       |---hook POST--->|   NimBLE NUS Server       |             |  snapshot / turn /  |
| Cursor Agent     |---hook POST--->|        +                  |             |  permission /       |
| Kiro CLI         |---hook POST--->|   AgentStateManager       |             |  文件夹推送          |
| opencode         |---hook POST--->|        +                  |             +---------------------+
+------------------+                |   Buddy HUD renderer      |
                                    |  +--------+--------+---+  |
                                    |  | 墨水屏  | LED  |按键|  |
                                    |  +--------+--------+---+  |
                                    +---------------------------+
```

### 固件模块

| 模块 | 说明 |
|--------|-------------|
| `DisplayManager` | 墨水屏驱动 (GxEPD2), 侧边栏布局, Buddy HUD, CJK 字体自动切换, 防残影 |
| `RGBLed` | WS2812 LED 灯效 (常亮, 呼吸, 闪烁, 彩虹, 渐灭) |
| `AgentStateManager` | 状态机, 12 种状态, 优先级解析, 多会话追踪 |
| `PermissionManager` | 权限请求队列 (最多 4 个), 来源标签 (BLE/HTTP), 超时自动拒绝 |
| `PixelArt` | 11 个 Clawd 螃蟹 XBM 位图 (48x48, 1-bit) |
| `WiFiManager` | WiFi STA 连接, AP 模式, 凭据存储 (NVS) |
| `InksPetWebServer` | AsyncWebServer, REST API, WebSocket, LittleFS 静态文件 |
| `buddy/BleBridge` | NimBLE Nordic UART Service, LE Secure Connections, passkey 配对 |
| `buddy/BuddyProtocol` | Hardware Buddy JSON 行解析, snapshot / turn / cmd / status ack |
| `buddy/BuddyStateMapper` | 官方 7 态 BLE 语义映射到 InksPet 12 态枚举 |
| `buddy/FileXfer` | 文件夹推送接收器, base64 块 → LittleFS, 热加载 GIF 渲染器 |
| `buddy/BuddyStats` | NVS 持久化 审批/拒绝/响应速度/等级/主人名/宠物名 |
| `buddy/EpaperGifRenderer` | AnimatedGIF 解码 → 1-bit Bayer 8×8 抖动帧缓冲 |
| `KeyManager` | 按键消抖 (50ms), 长按, 组合键检测 (A+C) |
| `BuzzerManager` | 无源蜂鸣器旋律 (权限提醒, 错误, 完成) |
| `BatteryManager` | ADC 电压读取, 电量百分比计算, USB 检测 |
| `TimeManager` | NTP 同步 (多服务器回退), 时区支持, BLE epoch 同步 |
| `ConfigManager` | NVS 持久化设置 (LED, 蜂鸣器, 权限, 免打扰, 时区) |
| `MemoryMonitor` | 堆内存追踪, 碎片化告警 |
| `Logger` | 带标签串口日志 (INFO/ERROR/WARNING/DEBUG) |

## API

### 智能体状态端点

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

### 支持的事件

| 事件 | 映射状态 |
|-------|-------------|
| `UserPromptSubmit` | thinking |
| `PreToolUse` | working |
| `PostToolUse` | working |
| `PostToolUseFailure` | error |
| `SubagentStart` (1-2 个会话) | juggling |
| `SubagentStart` (3+ 个会话) | conducting |
| `Stop` / `PostCompact` | completed |
| `PreCompact` | sweeping |
| `WorktreeCreate` | carrying |
| `PermissionRequest` | permission |

### 权限响应端点

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

可选操作: `allow`, `always_allow`, `deny`

### 状态查询

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

### 设备信息

```
GET /api/device/info
```

### 配置

```
GET /api/config
POST /api/config
POST /api/config/reset
```

## Hook 配置

### 简单方式

打开 InksPet Web 控制面板 `http://inkspet.local` (或设备 IP), 点击 **Hook Setup**, 复制提示词, 粘贴给你的 AI 编程助手。它会自行完成配置。10 秒搞定。

### 手动配置 (Claude Code)

在 `~/.claude/settings.json` 的 hooks 部分添加:

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

兼容所有支持 HTTP webhook 的 AI 工具: Claude Code、Copilot、Cursor、Codex、Gemini CLI、OpenCode。

## Claude 桌面 App 集成 (v1.1.0+)

InksPet 原生实现了 Anthropic 官方 [Hardware Buddy BLE 协议](https://github.com/anthropics/claude-desktop-buddy/blob/main/REFERENCE.md) -- 通过蓝牙 LE 与 Claude 桌面 App 配对后，它就成了一个一等公民的审批面板 + 实时会话展示器，不需要 HTTP hook。

### 配对步骤

1. Claude 桌面 App → **Help → Troubleshooting → Enable Developer Mode**
2. **Developer → Open Hardware Buddy...** → **Connect** → 从列表选择 `Claude-XXXX`
   (后缀是设备蓝牙 MAC 最后 2 字节的十六进制)
3. InksPet 墨水屏会显示 6 位配对码 → 在 macOS 弹窗中输入
4. 配对信息持久化, 下次自动重连

### 支持的能力

| 方向 | 能力 |
|-----------|------------|
| 桌面 → 设备 | 心跳快照 (total / running / waiting / msg / tokens / tokens_today / entries) |
| 桌面 → 设备 | 权限请求 (`id` / `tool` / `hint`) |
| 桌面 → 设备 | Turn 事件 (SDK content 数组, 文本 + 工具调用) |
| 桌面 → 设备 | 时间同步 (epoch + 时区偏移) |
| 桌面 → 设备 | 主人名、宠物名 |
| 桌面 → 设备 | 文件夹推送 -- 拖 GIF 字符包文件夹到 Hardware Buddy 窗口, 通过 BLE 流式传入, 写到 LittleFS, 热加载渲染器 |
| 设备 → 桌面 | 权限决策 `{cmd:permission, decision:once \| deny}` -- 物理按键 A 允许, C 拒绝 |
| 设备 → 桌面 | Status ack (电池 / heap / 运行时长 / 审批数 / 拒绝数 / 响应速度 / 等级 / bonded 标志) |
| 设备 → 桌面 | `unpair` -- Claude 侧按 "Forget" 时擦除已存 LE bond |

### 墨水屏 Buddy HUD

```
┌──────────────────────────────────────────┐
│ ■■■■■ │ approve: Bash                    │  ← 官方 msg (中文同样支持)
│ [像素 │ Today: 31.2K                     │  ← tokens_today 计数
│  画]  │ · · · · · · · · · · · · · · · ·  │
│       │ · 10:42 git push                 │  ← 最近 transcript (最多 3 条)
│ Work  │ · 10:41 yarn test                │
│       │ · 10:39 reading file...          │
├──────────────────────────────────────────┤
│ Claude-EA06                       bonded │  ← 设备名 · 加密状态
└──────────────────────────────────────────┘
```

### GIF 字符包

拖一个含 `manifest.json` + 每个 state `.gif` 文件的文件夹到 Hardware Buddy 窗口。字符包通过 BLE 流式传入 (base64 分块，每块独立 ack，上限 ~1.8 MB)，落到 `/characters/<name>/` LittleFS 目录，渲染器自动切换到动画模式。彩色 GIF 会用 8×8 Bayer 矩阵抖动成 1-bit -- 单色屏幕，彩色原作。

可参考 [上游仓库的 `bufo` 示例](https://github.com/anthropics/claude-desktop-buddy/tree/main/characters/bufo)。

### 安全

- **LE Secure Connections + MITM 配对** -- NUS 读写都是加密访问
- **DisplayOnly IO 能力** -- 6 位 passkey 每次开机重新生成, 在 macOS 端输入 (防被动嗅探)
- **`{cmd:unpair}` 处理** -- Claude 侧 "Forget" 会同步擦除设备端 bond

### 与 HTTP webhook 并行工作

BLE 与 HTTP 并存不冲突。Claude Code hook (HTTP) 和 Claude 桌面 App (BLE) 可以同时驱动 InksPet -- AgentStateManager 按优先级合并会话。权限请求带 **来源标签** (BLE vs HTTP)，按键决策永远在正确通道回复，不会串道。

## 编译构建

### 快速烧录 (浏览器)

访问 [inkspet.com/flash](https://inkspet.com/flash), 通过 USB-C 连接设备, 点击 Install。

### 从源码构建

```bash
# Build firmware + filesystem
python3 build.py

# Or step by step
pio run                  # Build firmware
pio run -t buildfs       # Build LittleFS filesystem

# 烧录到设备 (交互式菜单: 刷机 / 仅记录日志 / 备份 / 监控)
./flash_all.sh /dev/cu.usbserial-XXXXX

# 直接进入长时间监控仪表板
./flash_all.sh /dev/cu.usbserial-XXXXX -m

# 使用 .flash_config 保存的配置
./flash_all.sh /dev/cu.usbserial-XXXXX -c

# 查看帮助
./flash_all.sh -h
```

`flash_all.sh` 通过交互式菜单提供四种操作模式:

1. **刷写固件 / 文件系统** -- 完整刷新或仅固件, 可选是否擦除闪存
2. **记录设备日志** -- 串口输出保存到 `logs/serial_*.log`
3. **备份固件** -- 将完整 4MB 闪存导出到 `backups/` 目录
4. **长时间监控模式** -- 实时仪表板追踪重启、崩溃、内存错误、看门狗超时、WiFi 断开、Agent 事件、权限请求、墨水屏刷新、API 成功率和设备稳定性 (MTBF)。退出时自动生成 JSON 统计数据和摘要报告。

### 构建产物

`python3 build.py` 编译固件 + 文件系统, 将带版本号的二进制文件复制到 `firmware/`, 并生成 `manifest.json` 供 ESP Web Tools 使用。

## 技术栈

- **MCU**: ESP32 (Arduino framework, PlatformIO)
- **显示屏**: GxEPD2 + U8g2 bitmap fonts
- **网络**: ESPAsyncWebServer + WebSocket + mDNS
- **LED**: Adafruit NeoPixel (WS2812B)
- **存储**: LittleFS (Web 门户资源), NVS (配置)
- **Web 门户**: 原生 HTML/CSS/JS, 深色主题, 响应式

## 项目状态

**v1.1.0 -- Claude 桌面 App BLE 集成上线。** v1.0.x 所有功能之外，新增原生 Hardware Buddy 协议: LE Secure Connections 配对、实时会话 HUD、物理按键审批 Claude 的工具请求并回传桌面、GIF 字符包 1-bit 抖动播放、HUD 中文 / UTF-8 渲染。详见 [CHANGELOG.md](CHANGELOG.md)。

## 开源协议

MIT
