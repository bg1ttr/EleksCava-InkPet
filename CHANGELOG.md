# Changelog

All notable changes to InksPet firmware will be documented here.

## [1.1.1] - 2026-04-18

### Fixed
- **Status ack now accepted by Claude desktop** — the `{cmd:"status"}` response
  was silently dropped with "No response" in the Hardware Buddy window. Root
  cause: we included a non-standard `fw` field and omitted `bat.mA`. Now the
  ack strictly matches the REFERENCE.md schema (all documented keys present
  with zero defaults where InksPet lacks a sensor; no extra fields).
- Added diagnostic logging of the serialised ack payload so future schema
  drifts are visible in serial output.

---

## [1.1.0] - 2026-04-18

### Added
- **Bluetooth LE integration with Claude desktop Hardware Buddy**
  - NimBLE-based Nordic UART Service advertising as `Claude-XXXX`
  - Full implementation of the [claude-desktop-buddy BLE protocol](https://github.com/anthropics/claude-desktop-buddy/blob/main/REFERENCE.md):
    heartbeat snapshot, turn events, time sync, owner name, status ack, unpair, folder push
  - LE Secure Connections + MITM bonding with 6-digit passkey displayed on the e-paper
  - Physical-button permission flow — A/B/C answer tool requests, decision routed back
    as `{cmd:permission,decision:once|deny}` over BLE
- **Buddy HUD** on the e-paper — renders the upstream-authoritative `msg` one-liner,
  `Today: X.XK tokens`, and up to 3 recent transcript entries, with `bonded` indicator
- **GIF character pack support** — drag a pack folder into the Hardware Buddy window,
  device receives it over BLE, writes to LittleFS, hot-reloads the renderer; AnimatedGIF
  decoder pipes into a 1-bit Bayer-8×8 dithered framebuffer
- **Chinese text rendering** — HUD and permission screen auto-switch to
  `u8g2_font_wqy12_t_chinese2` when content contains multi-byte UTF-8; UTF-8-safe
  truncation prevents mid-codepoint garbage
- **Permission source tagging** — requests from BLE vs HTTP are tracked per-entry
  so button decisions reply back on the correct transport (no more cross-channel leakage)

### Changed
- `ArduinoJson` inbound buffer 4KB → 6KB to accommodate larger turn events
- `BuddySnapshot.msg` 24B → 64B; `promptHint` 44B → 64B
- HUD de-duplicates redraws when the snapshot fingerprint matches the last render

### Fixed
- Idle Claude Desktop snapshot (`running=0 waiting=0`) no longer fabricates a THINKING
  event — stale sessions age out via existing 30s session timeout
- UTF-8 text buffer overflow at multibyte boundaries no longer produces trailing
  replacement characters (`�`)

### Footprint
- Flash: 31% → 41% (NimBLE stack + Chinese font, still ample headroom)
- RAM: 24.9% unchanged — NimBLE and WiFi co-exist cleanly

---

## [1.0.2] - 2026-04-11

### Changed
- Version bump to validate release pipeline (no functional changes)

---

## [1.0.1] - 2026-04-11

### Fixed
- **E-paper display fade**: Replaced all partial refreshes with full refreshes to prevent contrast degradation over time
- **Redundant redraws**: Added content-change detection — display only refreshes when state, agent, tool, file, session count, or task progress actually changes
- **Elapsed time spam**: Removed 10-second periodic timer that triggered unnecessary full refreshes just to update the session clock
- **showTimeMode after wake**: `_forceFullRefresh` flag was ignored in time mode; now correctly applied
- **Double full-refresh after wake**: Waking from hibernate reset the 10-minute anti-ghosting timer to prevent an immediate redundant full refresh
- **Code duplication**: Extracted `buildAgentDisplayInfo()` helper — `onAgentStateChange()` and `refreshAgentStateDisplay()` now share a single implementation

### Changed
- Session elapsed time in sidebar is now shown **only when state is Completed** (total session duration), not during active work
- `deepClean()` (screen type transition) always performs the full black→white cycle instead of conditionally based on a partial count threshold
- `FULL_REFRESH_INTERVAL` reduced from 30 to 10 (used as deepClean cycle threshold)

### Removed
- `fullRefresh()` and `partialRefresh()` stub methods that did nothing useful
- `_partialCount` tracking (no longer needed with all-full-refresh strategy)

---

## [1.0.0] - 2026-03-01

Initial release of InksPet firmware.

### Features
- ESP32 + 2.9" e-paper display (296×128, SSD1680) AI agent status monitor
- 12 agent states with pixel art crab character (Claude Code mascot)
- WS2812B RGB LED status indicator with state-mapped colors and effects
- HTTP webhook API (`POST /api/agent/state`) for Claude Code hooks integration
- Physical button permission approval (Allow / Always Allow / Deny)
- Task progress tracking via `TodoWrite` hook (`POST /api/agent/tasks`)
- WiFi setup via captive portal AP mode
- mDNS hostname `inkspet.local`
- Web config portal (LittleFS) with hook setup guide
- E-paper hibernate after 30s idle; 10-minute periodic full refresh for anti-ghosting
- Battery voltage monitoring and DND mode
- OTA-ready firmware build pipeline with `build.py`
