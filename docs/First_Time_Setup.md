# InksPet First-Time Setup Guide

# InksPet 首次使用指南

> Your AI coding agent, alive on e-paper ✨
> 你的 AI 编程助手，在墨水屏上活了起来 ✨

---

Welcome to InksPet! You just got yourself a tiny hardware companion that watches your AI coding agents work in real-time. No more alt-tabbing to check if Claude is done. No more forgetting about that permission prompt buried in your terminal. InksPet sits on your desk, glowing and breathing with your agent's heartbeat.

欢迎来到 InksPet！你刚入手了一个桌面小伴侣，它能实时监视你的 AI 编程助手的工作状态。不用再 alt-tab 切窗口看 Claude 是否完成了。不用再忘记终端里埋着的权限审批请求。InksPet 安静地坐在你桌上，随着 Agent 的心跳呼吸发光。

---

## What's in the Box / 开箱清单

- **InksPet device** — EleksCava e-paper hardware with 2.9" e-ink display, 5 RGB LEDs, 3 buttons
- **USB-C cable** — for power and firmware flashing

That's it. No app to install, no account to create, no cloud service. Everything runs on your local network.

- **InksPet 设备** — EleksCava 电子墨水屏硬件，2.9 寸墨水屏 + 5 颗 RGB LED + 3 个按键
- **USB-C 数据线** — 用于供电和刷入固件

就这些。不需要安装 App，不需要注册账号，不需要云服务。一切都在你的局域网里运行。

---

## Step 1: Flash Firmware / 第一步：刷入固件

1. **Connect** your InksPet to your computer with the USB-C cable.
2. **Open** Chrome or Edge (Web Serial requires Chromium-based browsers).
3. **Go to** the web installer: `https://inkspet.elekscava.com/install` _(placeholder URL)_
4. **Click** "Install InksPet Firmware" and select the serial port.
5. **Wait** about 2 minutes for the flash to complete.
6. The screen will light up with the InksPet welcome screen when done!

---

1. 用 USB-C 数据线把 InksPet **连接**到电脑。
2. **打开** Chrome 或 Edge 浏览器（Web Serial 需要 Chromium 内核浏览器）。
3. **访问**在线刷机工具：`https://inkspet.elekscava.com/install` _(占位 URL)_
4. **点击** "Install InksPet Firmware"，选择对应的串口。
5. **等待**约 2 分钟，固件刷入完成。
6. 刷入成功后，墨水屏会亮起 InksPet 欢迎画面！

> **Alternative: PlatformIO** — If you prefer building from source:
> ```bash
> git clone https://github.com/user/EleksCava-InksPet.git
> cd EleksCava-InksPet
> pio run -t upload        # Flash firmware
> pio run -t uploadfs      # Flash web assets (LittleFS)
> ```

> **进阶选项：PlatformIO 编译** — 如果你喜欢从源码构建：
> ```bash
> git clone https://github.com/user/EleksCava-InksPet.git
> cd EleksCava-InksPet
> pio run -t upload        # 刷入固件
> pio run -t uploadfs      # 刷入 Web 资源 (LittleFS)
> ```

---

## Step 2: Connect to WiFi / 第二步：连接 WiFi

After flashing, InksPet enters WiFi setup mode automatically.

刷入固件后，InksPet 会自动进入 WiFi 配置模式。

### What you'll see on the screen / 屏幕上会显示：

```
+----------------------------------------------+
|                                              |
|   InksPet WiFi Setup                          |
|                                              |
|   Connect to: InksPet_A3F2                    |
|   Then open:  http://192.168.4.1             |
|                                              |
+----------------------------------------------+
     LED: white slow breathing
```

### Steps / 操作步骤：

1. **On your phone or laptop**, find the WiFi network named `InksPet_XXXX` (the XXXX is unique to your device). It's an open network, no password needed.

   在手机或电脑上，找到名为 `InksPet_XXXX` 的 WiFi 网络（XXXX 是你设备的唯一后缀）。这是一个开放网络，无需密码。

2. **A browser should open automatically** (captive portal). If not, manually go to `http://192.168.4.1`.

   浏览器应该会**自动弹出**配置页面（强制门户）。如果没有，手动访问 `http://192.168.4.1`。

3. **Select your WiFi network** from the scan list, enter the password, and hit Connect.

   从扫描列表中**选择你的 WiFi 网络**，输入密码，点击连接。

4. **InksPet restarts** and connects to your WiFi. The screen will show:

   InksPet 会**重启**并连接到你的 WiFi。屏幕会显示：

```
+----------------------------------------------+
|                                              |
|   InksPet Ready!                              |
|                                              |
|   IP: 192.168.1.42                           |
|   http://inkspet.local                        |
|                                              |
+----------------------------------------------+
     LED: green flash once, then off
```

> **Important**: InksPet only supports **2.4GHz WiFi**. If you only see 5GHz networks in the scan, check your router settings.
>
> **注意**：InksPet 仅支持 **2.4GHz WiFi**。如果扫描列表中只看到 5GHz 网络，请检查路由器设置。

---

## Step 3: Hook Up Your AI Agent / 第三步：连接你的 AI Agent

This is where the magic happens. You'll tell your AI coding agent to send status updates to InksPet via HTTP hooks.

接下来是最关键的一步。你需要让 AI 编程助手通过 HTTP Hook 把状态更新发送给 InksPet。

### Claude Code

Add the following to your `~/.claude/settings.json`:

将以下内容添加到 `~/.claude/settings.json`：

```json
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "type": "command",
        "command": "curl -s -X POST http://inkspet.local/api/agent/state -H 'Content-Type: application/json' -d '{\"agent\":\"claude-code\",\"event\":\"UserPromptSubmit\"}'"
      }
    ],
    "PreToolUse": [
      {
        "type": "command",
        "command": "curl -s -X POST http://inkspet.local/api/agent/state -H 'Content-Type: application/json' -d '{\"agent\":\"claude-code\",\"event\":\"PreToolUse\",\"tool\":\"'\"$CLAUDE_TOOL_NAME\"'\"}'"
      }
    ],
    "PostToolUse": [
      {
        "type": "command",
        "command": "curl -s -X POST http://inkspet.local/api/agent/state -H 'Content-Type: application/json' -d '{\"agent\":\"claude-code\",\"event\":\"PostToolUse\"}'"
      }
    ],
    "Stop": [
      {
        "type": "command",
        "command": "curl -s -X POST http://inkspet.local/api/agent/state -H 'Content-Type: application/json' -d '{\"agent\":\"claude-code\",\"event\":\"Stop\"}'"
      }
    ]
  }
}
```

> **Tip**: If `inkspet.local` doesn't resolve on your machine, replace it with the device IP shown on screen (e.g., `192.168.1.42`).
>
> **提示**：如果 `inkspet.local` 在你的电脑上无法解析，换成屏幕上显示的设备 IP（例如 `192.168.1.42`）。

### Copilot / Cursor / Codex / Other Agents / 其他 Agent

The principle is the same for any AI coding agent that supports hooks or plugins:

任何支持 Hook 或插件的 AI 编程助手都可以用相同的方式接入：

- Point the hook/webhook URL to `http://inkspet.local/api/agent/state`
- Send a JSON body with `agent`, `event`, and optionally `tool`, `file`, `session`
- See the [API Reference](#api-reference-for-advanced-users--api-参考进阶) below for the full event list

For reference, check how [Clawd on Desk](https://github.com/rullerzhou-afk/clawd-on-desk) integrates with various agents — InksPet uses the same event format, just a different URL.

参考 [Clawd on Desk](https://github.com/rullerzhou-afk/clawd-on-desk) 的各 Agent 集成方式 — InksPet 使用相同的事件格式，只是 URL 不同。

---

## Step 4: Start Coding! / 第四步：开始写代码！

That's it. Open your terminal, start Claude Code (or your AI agent of choice), and watch InksPet come alive.

搞定了。打开终端，启动 Claude Code（或你选择的 AI 编程助手），看 InksPet 活起来。

### What you'll see / 你会看到的：

- **You type a prompt** → InksPet's LED turns **blue** with a breathing effect. The pixel art character shows a thinking bubble. 💭

  **你输入提示词** → LED 变**蓝色**呼吸灯。像素画角色显示思考泡泡。

- **Agent starts editing files** → LED turns **green**, steady glow. Character switches to a typing pose. The screen shows which file is being edited.

  **Agent 开始编辑文件** → LED 变**绿色**常亮。角色切换到打字姿势。屏幕显示正在编辑的文件名。

- **Agent needs permission** → LED flashes **yellow**. Buzzer plays a short alert. Screen shows the permission request with button hints. Press **A** to allow!

  **Agent 请求权限** → LED **黄色**闪烁。蜂鸣器发出提示音。屏幕显示权限请求详情和按键提示。按 **A** 允许！

- **Agent finishes** → LED does a **yellow** fade-in-fade-out. Character celebrates. 🎉

  **Agent 完成** → LED **黄色**渐亮渐灭。角色庆祝动作。

- **Something goes wrong** → LED flashes **red** rapidly. Character shows a surprised face.

  **出错了** → LED **红色**快速闪烁。角色显示惊讶表情。

- **Nothing happening for 60 seconds** → LED goes off. Character falls asleep. Zero power draw from the e-paper (it keeps the image without power!).

  **60 秒无活动** → LED 熄灭。角色进入睡眠。墨水屏零功耗（断电也能保持画面！）。

---

## Button Guide / 按键指南

InksPet has 3 physical buttons: **A** (left), **B** (middle), **C** (right).

InksPet 有 3 个物理按键：**A**（左）、**B**（中）、**C**（右）。

### Normal Mode / 正常模式

| Button / 按键 | Short Press / 短按 | Long Press / 长按 (1s) |
|---|---|---|
| **A** | Switch to previous agent session / 切换到上一个 Agent 会话 | Show today's stats / 显示今日统计 |
| **B** | Switch between monitor & widget mode / 切换监控/小工具模式 | Toggle Do-Not-Disturb / 切换免打扰模式 |
| **C** | Switch to next agent session / 切换到下一个 Agent 会话 | — |
| **A + C** | _(hold 10s)_ Factory reset / 恢复出厂设置 | |

### Permission Mode / 权限审批模式

When a permission request is on screen:

当屏幕显示权限请求时：

| Button / 按键 | Action / 操作 | Description / 说明 |
|---|---|---|
| **A** | **Allow** / 允许 | Allow this one operation / 允许本次操作 |
| **B** | **Always Allow** / 始终允许 | Always allow this tool / 对该工具始终允许 |
| **C** | **Deny** / 拒绝 | Deny this operation / 拒绝本次操作 |

> If you don't press anything within 60 seconds, InksPet auto-denies the request (configurable in web dashboard).
>
> 如果 60 秒内没有按键操作，InksPet 自动拒绝请求（可在 Web 控制台中配置超时时间）。

---

## LED Color Guide / LED 颜色指南

You don't even need to look at the screen. The LED color in your peripheral vision tells the story.

你甚至不需要看屏幕。余光瞥到 LED 颜色就能知道状态。

| Color / 颜色 | Effect / 效果 | Meaning / 含义 |
|---|---|---|
| 🔵 Blue / 蓝色 | Breathing / 呼吸 | Agent is thinking / Agent 正在思考 |
| 🟢 Green / 绿色 | Steady / 常亮 | Agent is working (editing, running tools) / Agent 正在工作 |
| 🟡 Yellow / 黄色 | Flashing / 闪烁 | Permission needed — press a button! / 需要权限审批 — 按下按键！ |
| 🟡 Yellow / 黄色 | Fade in-out / 渐亮渐灭 | Task completed / 任务完成 |
| 🔴 Red / 红色 | Fast flash / 快闪 | Error occurred / 出现错误 |
| 🟣 Purple / 紫色 | Breathing / 呼吸 | Sub-agent spawned / 子 Agent 已启动 |
| 🟣 Purple / 紫色 | Steady / 常亮 | Multiple sub-agents running / 多个子 Agent 运行中 |
| 🔵 Cyan / 青色 | Steady / 常亮 | Context compaction (memory cleanup) / 上下文压缩 |
| 🟠 Orange / 橙色 | Steady / 常亮 | Worktree creation / 创建工作区 |
| ⚪ White / 白色 | Dim / 微亮 | Idle — agent connected but inactive / 空闲 — Agent 已连接但无活动 |
| ⚫ Off / 熄灭 | — | Sleeping or Do-Not-Disturb / 睡眠或免打扰模式 |

---

## Troubleshooting / 故障排除

**Can't find InksPet WiFi? / 找不到 InksPet WiFi？**
> Hold **A + C** for 10 seconds to force InksPet back into WiFi setup mode.
>
> 长按 **A + C** 10 秒，强制 InksPet 重新进入 WiFi 配置模式。

**LED not responding to agent events? / LED 不响应 Agent 事件？**
> - Make sure InksPet and your computer are on the **same WiFi network**
> - Test with: `curl -X POST http://inkspet.local/api/agent/state -H 'Content-Type: application/json' -d '{"agent":"test","event":"UserPromptSubmit"}'`
> - If `inkspet.local` doesn't work, try the device IP directly
>
> - 确保 InksPet 和你的电脑在**同一 WiFi 网络**下
> - 用这个命令测试：`curl -X POST http://inkspet.local/api/agent/state -H 'Content-Type: application/json' -d '{"agent":"test","event":"UserPromptSubmit"}'`
> - 如果 `inkspet.local` 不通，直接用设备 IP

**Permission requests not showing? / 权限请求不显示？**
> Check your hook configuration in `~/.claude/settings.json`. Make sure the URL matches your InksPet's address.
>
> 检查 `~/.claude/settings.json` 中的 hook 配置。确保 URL 与你 InksPet 的地址一致。

**Screen looks ghosted / has artifacts? / 屏幕有残影？**
> This is normal for e-paper after many partial refreshes. InksPet auto-cleans every 30 updates. You can also power-cycle the device.
>
> 这是墨水屏多次局部刷新后的正常现象。InksPet 每 30 次更新自动全屏刷新清除残影。也可以重新上电。

**Want to reconfigure WiFi? / 需要重新配置 WiFi？**
> Hold **A + C** for 10 seconds. InksPet clears WiFi config and re-enters setup mode.
>
> 长按 **A + C** 10 秒。InksPet 会清除 WiFi 配置并重新进入配置模式。

**Device keeps restarting? / 设备不停重启？**
> Make sure you're using a USB-C cable that supports data (not charge-only). Try a different port or cable.
>
> 确保使用的是支持数据传输的 USB-C 线（不是纯充电线）。换个接口或换条线试试。

---

## Web Dashboard / 网页控制台

Once InksPet is connected to your WiFi, you can access the web dashboard for configuration:

InksPet 连接 WiFi 后，你可以通过浏览器访问网页控制台进行配置：

**URL**: `http://inkspet.local` (or the device IP)

### What you can configure / 可配置项：

| Setting / 设置 | Description / 说明 | Default / 默认值 |
|---|---|---|
| LED brightness / LED 亮度 | Off / Low / Medium / High | Medium |
| LED color scheme / LED 配色方案 | State-to-color mapping / 状态配色映射 | Default |
| Buzzer / 蜂鸣器 | On / Off | On |
| Buzzer volume / 蜂鸣器音量 | Low / Medium / High | Low |
| Permission timeout / 权限超时 | Auto-deny after N seconds / N 秒后自动拒绝 | 60s |
| Default permission action / 默认权限操作 | Allow / Deny | Deny |
| Do-Not-Disturb / 免打扰模式 | On / Off | Off |
| Sleep timeout / 睡眠超时 | Seconds of inactivity before sleep / 无活动多久后睡眠 | 60s |
| Display orientation / 屏幕方向 | Normal / Flipped / 正常 / 翻转 | Normal |
| mDNS hostname / mDNS 主机名 | Device network name / 设备网络名 | inkspet |
| WiFi settings / WiFi 设置 | Change network / 切换网络 | — |

---

## API Reference (for Advanced Users) / API 参考（进阶）

InksPet exposes a simple REST API on port 80. All endpoints accept and return JSON.

InksPet 在端口 80 上提供简单的 REST API。所有端点接收和返回 JSON。

### POST `/api/agent/state` — Send agent event / 发送 Agent 事件

```json
{
  "agent": "claude-code",
  "session": "abc123",
  "event": "PreToolUse",
  "tool": "Edit",
  "file": "src/main.cpp",
  "timestamp": 1712700000
}
```

**Supported events / 支持的事件**:
`UserPromptSubmit`, `PreToolUse`, `PostToolUse`, `PostToolUseFailure`, `SubagentStart`, `Stop`, `PostCompact`, `PreCompact`, `WorktreeCreate`, `PermissionRequest`

### POST `/api/agent/permission/response` — Return permission decision / 返回权限决策

```json
{
  "session": "abc123",
  "action": "allow"
}
```

**Actions**: `allow`, `always_allow`, `deny`

### GET `/api/agent/status` — Query device state / 查询设备状态

**Response / 响应**:

```json
{
  "state": "thinking",
  "agent": "claude-code",
  "session": "abc123",
  "uptime": 3600,
  "active_sessions": 2,
  "sessions": [
    { "agent": "claude-code", "state": "thinking", "session": "abc123" },
    { "agent": "copilot", "state": "idle", "session": "def456" }
  ]
}
```

---

## What's Next / 接下来

- **Custom pixel art**: Upload your own 48x48 character via the web dashboard pixel art editor
- **Widget mode**: Press B to switch to clock / pomodoro timer when agents are idle
- **Activity stats**: Long-press A to see today's tool calls and permission stats

- **自定义像素画**：通过网页控制台的像素画编辑器上传你自己的 48x48 角色
- **小工具模式**：Agent 空闲时按 B 切换到时钟/番茄钟模式
- **活动统计**：长按 A 查看今日工具调用和权限审批统计

---

*InksPet by EleksCava — Your AI agent, alive on ink.* ✨

*InksPet by EleksCava — 让 AI Agent 在墨水屏上活起来。* ✨
