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

- **Claude デスクトップ BLE (v1.1.0+)** -- Anthropic 公式 [Hardware Buddy](https://github.com/anthropics/claude-desktop-buddy) プロトコルをネイティブサポート。LE Secure Connections でペアリング、セッションスナップショットをリアルタイム受信、物理ボタンでツール承認、GIF キャラクターパック受信 -- 下記 [Claude デスクトップ連携](#claude-デスクトップ連携-v110) を参照
- **12 種類のエージェント状態** -- ピクセルアート Clawd カニが状態ごとにユニークなポーズを表示
- **RGB LED ステータス** -- 青=思考中、緑=作業中、赤=エラー、黄=権限要求
- **物理権限ボタン** -- A/B/C ボタンで承認/拒否、ウィンドウ切り替え不要
- **中国語 / UTF-8 テキスト** -- HUD と権限画面で中国語のツール名、ファイルパス、Claude トランスクリプト概要を表示 (wqy12 フォント、約 3000 グリフ)
- **ツール呼び出し統計** -- Read/Write/Edit/Bash のリアルタイムカウント + 経過時間 + ファイルパス
- **ワンクリック Hook 設定** -- Web ダッシュボードでプロンプトをコピー&ペースト、AI が自動設定
- **おやすみモード** -- B ボタン長押しで LED + ブザーをミュート、権限要求を自動拒否
- **キャプティブポータル** -- AP モードでスマートフォンに WiFi 設定ページが自動表示
- **Web ダッシュボード** -- リアルタイム状態、デバイス設定、Hook 設定を `http://inkspet.local` で提供
- **マルチエージェント対応** -- 最大 8 つの同時 AI エージェントセッションを優先度解決で追跡
- **デュアルチャンネル** -- HTTP webhook (Claude Code / Copilot / Codex…) と Claude デスクトップ BLE が並行動作、ソースタグ付きで権限が正しい経路に返答
- **オープンソース** -- MIT ライセンス、ファームウェア + Web ポータル完全公開

## ハードウェア

[EleksCava](https://elekscava.com) 電子ペーパースマートデバイスをベースにしています:

- **MCU**: ESP32 (デュアルコア 240MHz, 320KB SRAM, **WiFi + Bluetooth LE 4.2**)
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
AI コーディングエージェント (HTTP)       EleksCava ハードウェア                   Claude デスクトップ (BLE)
+------------------+                +---------------------------+             +---------------------+
| Claude Code      |---hook POST--->|                           |<---BLE------|  Hardware Buddy     |
| Copilot CLI      |---hook POST--->|   ESP32 WebServer :80     |  NUS        |  (開発者モード)     |
| Codex CLI        |---hook POST--->|        +                  |             |                     |
| Gemini CLI       |---hook POST--->|   NimBLE NUS Server       |             |  snapshot / turn /  |
| Cursor Agent     |---hook POST--->|        +                  |             |  permission /       |
| Kiro CLI         |---hook POST--->|   AgentStateManager       |             |  フォルダプッシュ   |
| opencode         |---hook POST--->|        +                  |             +---------------------+
+------------------+                |   Buddy HUD renderer      |
                                    |  +--------+--------+---+  |
                                    |  |e-paper | LED  |ボタン| |
                                    |  +--------+--------+---+  |
                                    +---------------------------+
```

### ファームウェアモジュール

| モジュール | 説明 |
|--------|-------------|
| `DisplayManager` | 電子ペーパードライバー (GxEPD2)、サイドバーレイアウト、Buddy HUD、CJK フォント自動切替、残像防止 |
| `RGBLed` | WS2812 LED エフェクト (常時点灯、ブリージング、点滅、レインボー、フェード) |
| `AgentStateManager` | ステートマシン、12 状態、優先度解決、マルチセッション追跡 |
| `PermissionManager` | 権限要求キュー (最大 4)、ソースタグ (BLE/HTTP)、タイムアウト自動拒否 |
| `PixelArt` | 11 種類の Clawd カニ XBM ビットマップ (48x48, 1-bit) |
| `WiFiManager` | WiFi STA 接続、AP モード、認証情報保存 (NVS) |
| `InksPetWebServer` | AsyncWebServer、REST API、WebSocket、LittleFS 静的ファイル |
| `buddy/BleBridge` | NimBLE Nordic UART Service、LE Secure Connections、passkey ボンディング |
| `buddy/BuddyProtocol` | Hardware Buddy JSON 行パーサ、snapshot / turn / cmd / status ack |
| `buddy/BuddyStateMapper` | 公式 7 状態 BLE セマンティクスを InksPet の 12 状態列挙にマップ |
| `buddy/FileXfer` | フォルダプッシュレシーバ、base64 チャンク → LittleFS、GIF レンダラーをホットリロード |
| `buddy/BuddyStats` | NVS 永続化 承認 / 拒否 / 応答速度 / レベル / オーナー名 / ペット名 |
| `buddy/EpaperGifRenderer` | AnimatedGIF デコーダ → 1-bit Bayer 8×8 ディザリングフレームバッファ |
| `KeyManager` | ボタンデバウンス (50ms)、長押し、コンボ検出 (A+C) |
| `BuzzerManager` | パッシブブザーメロディ (権限アラート、エラー、完了) |
| `BatteryManager` | ADC 電圧読み取り、パーセンテージ計算、USB 検出 |
| `TimeManager` | NTP 同期 (マルチサーバーフォールバック)、タイムゾーン対応、BLE epoch 同期 |
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

## Claude デスクトップ連携 (v1.1.0+)

InksPet は Anthropic 公式の [Hardware Buddy BLE プロトコル](https://github.com/anthropics/claude-desktop-buddy/blob/main/REFERENCE.md) をネイティブ実装しています -- Bluetooth LE で Claude デスクトップアプリとペアリングすれば、HTTP hook なしで一級市民の承認パネル兼リアルタイムセッション表示になります。

### ペアリング手順

1. Claude デスクトップ App → **Help → Troubleshooting → Enable Developer Mode**
2. **Developer → Open Hardware Buddy…** → **Connect** → リストから `Claude-XXXX` を選択
   (サフィックスはデバイスの Bluetooth MAC の最後 2 バイト)
3. InksPet が電子ペーパーに 6 桁のパスキーを表示 → macOS の入力プロンプトにタイプ
4. ボンド情報は再起動後も保持、次回以降は自動再接続

### サポート機能

| 方向 | 機能 |
|-----------|------------|
| デスクトップ → デバイス | ハートビートスナップショット (total / running / waiting / msg / tokens / tokens_today / entries) |
| デスクトップ → デバイス | 権限要求 (`id` / `tool` / `hint`) |
| デスクトップ → デバイス | Turn イベント (SDK content 配列、テキスト + ツール呼び出し) |
| デスクトップ → デバイス | 時刻同期 (epoch + タイムゾーンオフセット) |
| デスクトップ → デバイス | オーナー名、ペット名 |
| デスクトップ → デバイス | フォルダプッシュ -- GIF キャラクターパックのフォルダを Hardware Buddy ウィンドウにドラッグ、BLE でストリーミング、LittleFS に書き込み、レンダラーをホットスワップ |
| デバイス → デスクトップ | 権限決定 `{cmd:permission, decision:once \| deny}` -- 物理ボタン A で承認、C で拒否 |
| デバイス → デスクトップ | Status ack (バッテリー / heap / 稼働時間 / 承認数 / 拒否数 / 速度 / レベル / bonded フラグ) |
| デバイス → デスクトップ | `unpair` -- Claude 側の "Forget" 押下で保存済み LE bond を消去 |

### 電子ペーパー Buddy HUD

```
┌──────────────────────────────────────────┐
│ ■■■■■ │ approve: Bash                    │  ← 公式 msg (中国語も対応)
│ [ピク │ Today: 31.2K                     │  ← tokens_today カウンタ
│  セル]│ · · · · · · · · · · · · · · · ·  │
│       │ · 10:42 git push                 │  ← 直近トランスクリプト (最大 3)
│ Work  │ · 10:41 yarn test                │
│       │ · 10:39 reading file...          │
├──────────────────────────────────────────┤
│ Claude-EA06                       bonded │  ← デバイス名 · リンク暗号化状態
└──────────────────────────────────────────┘
```

### GIF キャラクターパック

`manifest.json` と state ごとの `.gif` ファイルを含むフォルダを Hardware Buddy ウィンドウにドラッグ。パックは BLE でストリーミング (base64 チャンク + チャンクごとの ack、上限約 1.8 MB)、LittleFS の `/characters/<name>/` に保存、レンダラーがアニメーションモードに切り替わります。カラー GIF は 8×8 Bayer マトリクスで 1-bit にディザリング -- モノクロパネルでカラー原作の味わい。

アップストリーム repo の [`bufo` サンプル](https://github.com/anthropics/claude-desktop-buddy/tree/main/characters/bufo) を参照。

### セキュリティ

- **LE Secure Connections + MITM ボンディング** -- NUS の読み書きは暗号化必須
- **DisplayOnly IO capability** -- 6 桁 passkey は起動毎にデバイス側で生成、macOS 側で入力 (パッシブ BLE スニッフに耐性)
- **`{cmd:unpair}` ハンドリング** -- Claude の "Forget" ボタンがデバイスのボンドも同期消去

### HTTP webhook と並行動作

BLE と HTTP は競合なしで共存。Claude Code フック (HTTP) と Claude デスクトップ (BLE) は同時に InksPet を駆動できます -- AgentStateManager が優先度でセッションをマージ。権限要求は **ソースタグ** (BLE vs HTTP) を持つため、ボタンによる決定は常に正しいトランスポートで応答され、チャンネル間リークは発生しません。

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

# デバイスにフラッシュ (対話式メニュー: 書き込み / ログ記録 / バックアップ / モニター)
./flash_all.sh /dev/cu.usbserial-XXXXX

# 長時間モニターダッシュボードに直接入る
./flash_all.sh /dev/cu.usbserial-XXXXX -m

# .flash_config に保存された設定を使用
./flash_all.sh /dev/cu.usbserial-XXXXX -c

# ヘルプを表示
./flash_all.sh -h
```

`flash_all.sh` は対話式メニューから 4 つの操作モードを提供します:

1. **ファームウェア / ファイルシステムの書き込み** -- 完全書き込みまたはファームウェアのみ、フラッシュ消去オプション付き
2. **デバイスログ記録** -- シリアル出力を `logs/serial_*.log` に保存
3. **ファームウェアのバックアップ** -- 4MB フラッシュ全体を `backups/` にダンプ
4. **長時間モニターモード** -- リアルタイムダッシュボードで再起動、クラッシュ、メモリエラー、ウォッチドッグタイムアウト、WiFi 切断、エージェントイベント、権限要求、電子ペーパーリフレッシュ、API 成功率、デバイス安定性 (MTBF) を追跡。終了時に JSON 統計とサマリーレポートを自動生成。

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

**v1.1.0 -- Claude デスクトップ BLE 連携ライブ。** v1.0.x 全機能に加え、Hardware Buddy プロトコルをネイティブ実装: LE Secure Connections ペアリング、リアルタイムセッション HUD、物理ボタンで Claude デスクトップにツール承認を返信、1-bit ディザリング GIF キャラクターパック再生、HUD 上の中国語 / UTF-8 レンダリング。詳細は [CHANGELOG.md](CHANGELOG.md) を参照。

## ライセンス

MIT
