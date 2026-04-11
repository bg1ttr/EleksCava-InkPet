# Changelog

All notable changes to InksPet firmware will be documented here.

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
