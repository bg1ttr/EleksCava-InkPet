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

- **12 种智能体状态** -- 像素风 Clawd 螃蟹，每种状态独特姿态
- **RGB LED 状态灯** -- 蓝色=思考中，绿色=工作中，红色=错误，黄色=待授权
- **物理授权按键** -- 按 A/B/C 批准/拒绝，无需切换窗口
- **工具调用统计** -- 实时 Read/Write/Edit/Bash 计数 + 已用时间 + 文件路径
- **一键 Hook 配置** -- Web 面板提供复制粘贴提示词，AI 自行完成配置
- **免打扰模式** -- 长按 B 键静音 LED + 蜂鸣器，自动拒绝权限请求
- **强制门户** -- AP 模式下手机自动弹出 WiFi 配置页面
- **Web 控制面板** -- 实时状态、设备配置、Hook 配置，访问 `http://inkspet.local`
- **多智能体支持** -- 同时追踪最多 8 个 AI 智能体会话，优先级自动解析
- **开源** -- MIT 协议，完整固件 + Web 门户

## 硬件

基于 [EleksCava](https://elekscava.com) 墨水屏智能设备:

- **MCU**: ESP32 (双核 240MHz, 320KB SRAM, WiFi)
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

### 固件模块

| 模块 | 说明 |
|--------|-------------|
| `DisplayManager` | 墨水屏驱动 (GxEPD2), 侧边栏布局, 防残影, 字体渲染 |
| `RGBLed` | WS2812 LED 灯效 (常亮, 呼吸, 闪烁, 彩虹, 渐灭) |
| `AgentStateManager` | 状态机, 12 种状态, 优先级解析, 多会话追踪 |
| `PermissionManager` | 权限请求队列 (最多 4 个), 超时自动拒绝, 按键审批 |
| `PixelArt` | 11 个 Clawd 螃蟹 XBM 位图 (48x48, 1-bit) |
| `WiFiManager` | WiFi STA 连接, AP 模式, 凭据存储 (NVS) |
| `InksPetWebServer` | AsyncWebServer, REST API, WebSocket, LittleFS 静态文件 |
| `KeyManager` | 按键消抖 (50ms), 长按, 组合键检测 (A+C) |
| `BuzzerManager` | 无源蜂鸣器旋律 (权限提醒, 错误, 完成) |
| `BatteryManager` | ADC 电压读取, 电量百分比计算, USB 检测 |
| `TimeManager` | NTP 同步 (多服务器回退), 时区支持 |
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

**v1.0.0 -- 固件功能完备。** 智能体状态显示、权限审批、LED 同步、工具统计、Web 控制面板一键 Hook 配置均已实现。下一步: 外壳设计和小批量生产。

## 开源协议

MIT
