[English](README.md) | [中文](README.zh-CN.md) | **日本語**

# InksPet by EleksCava

> AI コーディングエージェントが、電子ペーパーで息づく。**EleksCava x Claude**

InksPet は、お使いの [EleksCava](https://elekscava.com) 電子ペーパーデバイスをデスクトップ AI エージェントモニターに変えます。Claude Code、Copilot、Codex などの AI コーディングエージェントが考え、入力し、構築する様子を、2.9 インチ電子インクディスプレイと RGB LED ステータスライトでリアルタイムに確認できます。

公式サイト: [inkspet.com](https://inkspet.com) | デバイス購入: [elekscava.com](https://elekscava.com)

## なぜ InksPet + EleksCava なのか？

InksPet はファームウェアです。[EleksCava](https://elekscava.com) はハードウェアです。この組み合わせにより、デスクトップ電子ペーパーデバイスが AI エージェントのパートナーになります。

| ソフトウェアエージェントモニター | InksPet on EleksCava |
|------------------------|---------------------|
| 画面スペースを占有する | 専用デバイスで画面スペースはゼロ |
| ウィンドウの裏に隠れやすい | 電子ペーパーは常に表示、LED は視界の端で感知可能 |
| 物理的なフィードバックがない | RGB LED で状態を一目で把握 |
| クリックして権限を承認 | 物理ボタンでウィンドウ切り替え不要 |
| マシン上のもう一つのアプリ | 低消費電力の電子インクデバイス、常時オン |

## 機能

- **12 種類のエージェント状態** -- ピクセルアート Clawd カニが状態ごとにユニークなポーズを表示
- **RGB LED ステータス** -- 青=思考中、緑=作業中、赤=エラー、黄=権限要求
- **物理権限ボタン** -- A/B/C ボタンで承認/拒否、ウィンドウ切り替え不要
- **ツール呼び出し統計** -- Read/Write/Edit/Bash のリアルタイムカウント + 経過時間 + ファイルパス
- **ワンクリック Hook 設定** -- Web ダッシュボードでプロンプトをコピー&ペースト、AI が自動設定
- **おやすみモード** -- B ボタン長押しで LED + ブザーをミュート、権限要求を自動拒否
- **キャプティブポータル** -- AP モードでスマートフォンに WiFi 設定ページが自動表示
- **Web ダッシュボード** -- リアルタイム状態、デバイス設定、Hook 設定を `http://inkspet.local` で提供
- **マルチエージェント対応** -- 最大 8 つの同時 AI エージェントセッションを優先度解決で追跡
- **オープンソース** -- MIT ライセンス、ファームウェア + Web ポータル完全公開

## ハードウェア

[EleksCava](https://elekscava.com) 電子ペーパースマートデバイスをベースにしています:

- **MCU**: ESP32 (デュアルコア 240MHz, 320KB SRAM, WiFi)
- **ディスプレイ**: 2.9 インチ電子ペーパー (296 x 128 px, 白黒, 常時表示)
- **LED**: WS2812B RGB x5 (フロストアクリルディフューザー)
- **ボタン**: 物理キー 3 個 (A / B / C)
- **ブザー**: パッシブブザー (デフォルトオフ、設定可能)
- **バッテリー**: 3.7V リチウムポリマー、USB-C 充電
- **電源**: USB-C

## 12 種類のエージェント状態

目安: **青 = 思考中、緑 = 作業中、赤 = エラー、黄 = 操作が必要**

| 状態 | ピクセルアート | LED 色 | LED 効果 | トリガー |
|-------|-----------|-----------|------------|---------|
| スリープ | 丸まって zzz | オフ | -- | 60 秒以上アイドル |
| アイドル | リラックスポーズ | 白(暗) | 常時点灯 | アクティビティなし |
| 思考中 | 吹き出し | 青 | ブリージング | `UserPromptSubmit` |
| 作業中 | キーボードポーズ | 緑 | 常時点灯 | `PreToolUse` / `PostToolUse` |
| 完了 | お祝い | 緑 | フェード 1 回 | `Stop` / `PostCompact` |
| エラー | 驚いた顔、! | 赤 | 高速点滅 | `PostToolUseFailure` |
| 権限要求 | はてなマーク | 黄 | 点滅 | `PermissionRequest` |
| ジャグリング | ボール投げ | 紫 | ブリージング | `SubagentStart` (1-2 セッション) |
| 指揮中 | 指揮棒 | 紫 | 常時点灯 | `SubagentStart` (3+ セッション) |
| 清掃中 | ほうき | シアン | ブリージング | `PreCompact` |
| 運搬中 | 箱を持つ | オレンジ | 常時点灯 | `WorktreeCreate` |

## 権限承認

AI エージェントがツール権限を要求すると、電子ペーパーに以下が表示されます:

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

**A** で許可、**B** で常に許可、**C** で拒否 -- ウィンドウの切り替えは不要です。

## アーキテクチャ

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

### ファームウェアモジュール

| モジュール | 説明 |
|--------|-------------|
| `DisplayManager` | 電子ペーパードライバー (GxEPD2)、サイドバーレイアウト、残像防止、フォントレンダリング |
| `RGBLed` | WS2812 LED エフェクト (常時点灯、ブリージング、点滅、レインボー、フェード) |
| `AgentStateManager` | ステートマシン、12 状態、優先度解決、マルチセッション追跡 |
| `PermissionManager` | 権限要求キュー (最大 4)、タイムアウト自動拒否、ボタン承認 |
| `PixelArt` | 11 種類の Clawd カニ XBM ビットマップ (48x48, 1-bit) |
| `WiFiManager` | WiFi STA 接続、AP モード、認証情報保存 (NVS) |
| `InksPetWebServer` | AsyncWebServer、REST API、WebSocket、LittleFS 静的ファイル |
| `KeyManager` | ボタンデバウンス (50ms)、長押し、コンボ検出 (A+C) |
| `BuzzerManager` | パッシブブザーメロディ (権限アラート、エラー、完了) |
| `BatteryManager` | ADC 電圧読み取り、パーセンテージ計算、USB 検出 |
| `TimeManager` | NTP 同期 (マルチサーバーフォールバック)、タイムゾーン対応 |
| `ConfigManager` | NVS 永続設定 (LED、ブザー、権限、おやすみモード、タイムゾーン) |
| `MemoryMonitor` | ヒープ追跡、フラグメンテーションアラート |
| `Logger` | タグ付きシリアルログ (INFO/ERROR/WARNING/DEBUG) |

## API

### エージェント状態エンドポイント

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

### サポートされるイベント

| イベント | マッピング先の状態 |
|-------|-------------|
| `UserPromptSubmit` | thinking |
| `PreToolUse` | working |
| `PostToolUse` | working |
| `PostToolUseFailure` | error |
| `SubagentStart` (1-2 セッション) | juggling |
| `SubagentStart` (3+ セッション) | conducting |
| `Stop` / `PostCompact` | completed |
| `PreCompact` | sweeping |
| `WorktreeCreate` | carrying |
| `PermissionRequest` | permission |

### 権限レスポンスエンドポイント

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

アクション: `allow`, `always_allow`, `deny`

### ステータス照会

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

### デバイス情報

```
GET /api/device/info
```

### 設定

```
GET /api/config
POST /api/config
POST /api/config/reset
```

## Hook 設定

### 簡単な方法

InksPet Web ダッシュボード `http://inkspet.local` (またはデバイス IP) を開き、**Hook Setup** をクリックし、プロンプトをコピーして AI コーディングアシスタントに貼り付けてください。AI が自動的に設定を完了します。わずか 10 秒です。

### 手動設定 (Claude Code)

`~/.claude/settings.json` の hooks セクションに以下を追加してください:

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

HTTP webhook をサポートするすべての AI ツールで動作します: Claude Code、Copilot、Cursor、Codex、Gemini CLI、OpenCode。

## ビルド

### クイックフラッシュ (ブラウザ)

[inkspet.com/flash](https://inkspet.com/flash) にアクセスし、USB-C でデバイスを接続して Install をクリックしてください。

### ソースからビルド

```bash
# Build firmware + filesystem
python3 build.py

# Or step by step
pio run                  # Build firmware
pio run -t buildfs       # Build LittleFS filesystem

# Flash to device
./flash_all.sh /dev/cu.usbserial-XXXXX      # Full flash
./flash_all.sh /dev/cu.usbserial-XXXXX -f   # Firmware only
./flash_all.sh /dev/cu.usbserial-XXXXX -e   # Erase + full flash
./flash_all.sh /dev/cu.usbserial-XXXXX -m   # Serial monitor
```

### ビルド成果物

`python3 build.py` はファームウェア + ファイルシステムをコンパイルし、バージョン付きバイナリを `firmware/` にコピーし、ESP Web Tools 用の `manifest.json` を生成します。

## 技術スタック

- **MCU**: ESP32 (Arduino framework, PlatformIO)
- **ディスプレイ**: GxEPD2 + U8g2 bitmap fonts
- **ネットワーク**: ESPAsyncWebServer + WebSocket + mDNS
- **LED**: Adafruit NeoPixel (WS2812B)
- **ストレージ**: LittleFS (Web ポータルアセット)、NVS (設定)
- **Web ポータル**: バニラ HTML/CSS/JS、ダークテーマ、レスポンシブ

## ステータス

**v1.0.0 -- ファームウェア機能完成。** エージェント状態表示、権限承認、LED 同期、ツール統計、Web ダッシュボードによるワンクリック Hook 設定がすべて動作しています。次のステップ: 筐体設計と少量生産。

## ライセンス

MIT
