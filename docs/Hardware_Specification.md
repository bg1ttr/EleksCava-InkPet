# InksPet Hardware Specification

> Based on [EleksCava E-Paper Smart Device](https://github.com/bg1ttr/EleksCava-E-Paper-Smart-Device) Gen2 hardware platform.
> Source reference: `EleksCava-E-Paper-Smart-Device/docs/Hardware_Specification.md`

---

## 1. Hardware Overview

### 1.1 MCU

| Parameter | Value |
|-----------|-------|
| Chip | ESP32-WROOM-32 |
| CPU | Dual-core Xtensa LX6 @ 240MHz |
| RAM | ~320KB SRAM (no PSRAM) |
| Flash | 4MB (custom partition table) |
| WiFi | 802.11 b/g/n, 2.4GHz |
| Framework | Arduino (ESP32 Arduino Core) |
| Platform | espressif32 (PlatformIO) |

### 1.2 Power

| Parameter | Value |
|-----------|-------|
| Battery | 3.7V 1200mAh Li-Po (Model 603450) |
| Charging IC | TP4056 with protection circuit |
| Interface | USB-C (power + programming + serial) |
| Brownout Detection | Level 5 (0-7 scale) |

---

## 2. GPIO Pin Map

### 2.1 Complete Pin Summary

| GPIO | Function | Direction | Notes |
|------|----------|-----------|-------|
| 2 | LED_BUILTIN | Output | Onboard LED (unused in InksPet) |
| 5 | KEY_C | Input | Right button, internal pull-up |
| 12 | EPD_BUSY | Input | E-paper busy signal (active low) |
| 13 | KEY_B | Input | Middle button, internal pull-up |
| 14 | EPD_RST | Output | E-paper hardware reset (active low) |
| 16 | RGB_LED | Output | WS2812B data line |
| 17 | USB_STATUS | Input | USB power detection (ADC) |
| 18 | EPD_SCK | Output | SPI clock |
| 23 | EPD_MOSI | Output | SPI data out |
| 25 | BUZZER | Output | Passive buzzer (PWM) |
| 26 | EPD_CS | Output | E-paper chip select (active low) |
| 27 | EPD_DC | Output | E-paper data/command select |
| 32 | BATT_ADC | Input | Battery voltage sense (ADC, divider ratio 2.21:1) |
| 33 | KEY_A | Input | Left button, internal pull-up |

### 2.2 E-Paper Display (SPI Bus)

```cpp
#define EPD_SCK     18   // SPI Clock
#define EPD_MOSI    23   // SPI Data Out
#define EPD_CS      26   // Chip Select
#define EPD_RST     14   // Hardware Reset
#define EPD_DC      27   // Data/Command
#define EPD_BUSY    12   // Busy Status

// SPI init (no MISO needed for e-paper)
SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
```

### 2.3 Buttons

```cpp
const uint8_t KEY_A = 33;  // Left button
const uint8_t KEY_B = 13;  // Middle button
const uint8_t KEY_C = 5;   // Right button

// All buttons: INPUT_PULLUP, active LOW (pressed = LOW)
```

| Parameter | Value |
|-----------|-------|
| Debounce delay | 50ms |
| Long press duration | 1000ms |
| Factory reset combo | A + C held 10 seconds |

### 2.4 RGB LED

```cpp
#define RGB_LED_PIN  16
```

| Parameter | Value |
|-----------|-------|
| Type | WS2812B (NeoPixel compatible) |
| Count | 5 LEDs |
| Color order | GRB |
| Frequency | 800KHz |
| Library | Adafruit NeoPixel |

### 2.5 Buzzer

```cpp
#define PIN_BUZZER  25
#define BUZZER_CHANNEL 0  // LEDC PWM channel
```

| Parameter | Value |
|-----------|-------|
| Type | Passive (PWM driven) |
| PWM frequency | 5kHz |
| Resolution | 8-bit duty cycle |

### 2.6 Battery Monitoring

```cpp
static const uint8_t BATT_ADC_PIN = 32;     // Battery voltage ADC
static const uint8_t USB_STATUS_PIN = 17;    // USB power detection
static constexpr float VOLTAGE_DIVIDER_RATIO = 2.21f;
static constexpr float BATT_MIN_VOLTAGE = 3.0f;
static constexpr float BATT_MAX_VOLTAGE = 4.2f;
```

---

## 3. E-Paper Display

### 3.1 Specifications

| Parameter | Value |
|-----------|-------|
| Size | 2.9 inch |
| Resolution | 296 x 128 pixels |
| Controller | SSD1680 (GDEY029T94 compatible) |
| Driver class | GxEPD2_290_T94 |
| Color depth | 1-bit (black / white) |
| Library | GxEPD2 + U8g2_for_Adafruit_GFX |

### 3.2 Display Initialization

```cpp
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

#define DISP_WIDTH  296
#define DISP_HEIGHT 128

GxEPD2_BW<GxEPD2_290_T94, DISP_HEIGHT> display(
    GxEPD2_290_T94(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;
u8g2Fonts.begin(display);
```

### 3.3 Refresh Characteristics

| Refresh Type | Duration | Cooldown | Use Case |
|-------------|----------|----------|----------|
| Full refresh | ~1-2s | 3000ms min | Boot, mode switch, anti-ghosting |
| Partial refresh | ~300ms | 800ms min | Content updates |

- Force full refresh every 30 partial updates to prevent ghosting
- Use mutex protection in multi-threaded context
- Avoid calling `display->powerOff()` (causes flash artifacts)

### 3.4 Display Coordinate System

```
(0,0)                              (295,0)
  +--------------------------------------+
  |                                      |
  |          296 x 128 pixels            |
  |                                      |
  +--------------------------------------+
(0,127)                            (295,127)
```

### 3.5 InksPet Display Layout (Planned)

```
+----------------------------------------------+
|                                              |
| [48x48 pixel art]  State: thinking           |  Top: Character + state
|                                              |
+----------------------------------------------+
| Agent: Claude Code                           |  Bottom: Agent info
| Tool: Edit src/main.cpp                      |
| 3 tools queued                               |
+----------------------------------------------+
```

---

## 4. Memory Constraints

### 4.1 RAM Budget

| Component | Typical Usage |
|-----------|---------------|
| WiFi/TCP stack | ~50-60KB |
| SSL/TLS (per connection) | ~30-40KB |
| Display buffer | ~5KB |
| AsyncWebServer + WebSocket | ~20KB |
| NVS, FreeRTOS, drivers | ~30KB |
| **Free heap target** | **>80KB** |
| **Critical threshold** | **40KB** |

### 4.2 Key Memory Considerations for InksPet

- **Only one SSL connection at a time**: ESP32 heap fragments after first HTTPS request, `maxBlock` drops from ~90KB to ~39KB permanently
- **Release SSL before switching tasks**: Call `secureClient.reset()` to reclaim contiguous memory
- **Pixel art assets**: Store as XBM/bitmap in PROGMEM or LittleFS, not runtime-generated
- **WebSocket**: Keep payload small for agent state updates

### 4.3 Heap Monitoring

```cpp
size_t freeHeap = ESP.getFreeHeap();       // Total free bytes
size_t maxBlock = ESP.getMaxAllocHeap();   // Largest contiguous block

if (freeHeap < 40000) {
    // Emergency cleanup
}
```

---

## 5. Flash Partition Table

| Name | Type | Offset | Size | Description |
|------|------|--------|------|-------------|
| nvs | data | 0x9000 | 64KB | Key-value storage (WiFi, config) |
| otadata | data | 0x19000 | 8KB | OTA metadata |
| app0 | app | 0x20000 | 2.7MB | Main firmware |
| spiffs | data | 0x2C0000 | 700KB | LittleFS (pixel art, web assets) |
| coredump | data | 0x36F000 | 128KB | Core dump storage |
| future | data | 0x38F000 | 452KB | Reserved |

---

## 6. WiFi

### 6.1 Connection Flow

```
Power On
  |
  v
Check NVS for saved WiFi config
  |
  +-- No config --> AP Mode (EleksCava_XXXX, open, 192.168.4.1)
  |
  +-- Has config --> Auto connect (30s timeout)
                       |
                       +-- Success --> Normal operation
                       +-- Fail --> Retry / AP Mode fallback
```

### 6.2 AP Mode

| Parameter | Value |
|-----------|-------|
| SSID | EleksCava_XXXX (MAC suffix) |
| Password | None (open network) |
| IP | 192.168.4.1 |
| DNS | Captive portal redirect |

### 6.3 mDNS

Device accessible at `http://elekscava.local` after WiFi connection (port 80).

---

## 7. Serial Interface

| Parameter | Value |
|-----------|-------|
| Baud rate | 115200 |
| Format | 8-N-1 |
| Connection | USB-C |

### Debug Commands

| Command | Description |
|---------|-------------|
| `get_status` | Returns "device_ready" |
| `memcheck` | Memory diagnostics report |
| `viewlog` / `clearlog` | View / clear device log |
| `factory_test` | Enter factory test mode |

---

## 8. Build Configuration

### 8.1 PlatformIO

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
board_build.partitions = custom_partitions.csv
board_build.filesystem = littlefs
```

### 8.2 Key Build Flags

```ini
build_flags =
    -Os                                       # Size optimization
    -DGXEPD2_DISPLAY_CLASS=GxEPD2_BW
    -DGXEPD2_DRIVER_CLASS=GxEPD2_290_T94
    -DCONFIG_ASYNC_TCP_RUNNING_CORE=1
    -DCONFIG_ASYNC_TCP_USE_WDT=0
    -DCONFIG_MBEDTLS_SSL_IN_CONTENT_LEN=2048  # Reduced SSL buffer
    -DCONFIG_MBEDTLS_SSL_OUT_CONTENT_LEN=2048
    -DCONFIG_MBEDTLS_DYNAMIC_BUFFER=1
    -DCONFIG_ESP32_BROWNOUT_DET_LVL=5
```

### 8.3 Library Dependencies

```ini
lib_deps =
    bblanchon/ArduinoJson@^6.21.3
    esphome/ESPAsyncWebServer-esphome
    olikraus/U8g2_for_Adafruit_GFX@^1.8.0

# Bundled in lib/:
#   GxEPD2          - E-paper driver
#   Adafruit_NeoPixel - RGB LED driver
```

---

## 9. Development Notes

### 9.1 Power

- Reduce LED brightness during boot to prevent brownout
- Add 500ms delay before WiFi init for power stabilization
- Use 80MHz CPU during WiFi initialization, restore to 240MHz after

### 9.2 Display

- Always use mutex (`xSemaphoreTake`) for display operations
- Respect cooldown periods between refreshes
- Force full refresh every ~30 partial updates
- E-paper retains image without power (ideal for "sleeping" state)

### 9.3 SSL / HTTPS

- Use explicit `WiFiClientSecure` with `begin(*client, url)` to avoid hidden memory fragmentation
- Release SSL client (`secureClient.reset()`) when switching tasks
- Check `ESP.getMaxAllocHeap() > 40000` before SSL connections

### 9.4 InksPet-Specific Considerations

- **Agent state webhook latency**: AsyncWebServer handles POST within ~1ms, but e-paper refresh takes 300ms-2s. Buffer state changes, display latest.
- **Permission approval timeout**: AI agents may timeout waiting for button press. Consider auto-deny after configurable timeout (e.g., 30s).
- **Pixel art storage**: 12 states x 48x48px x 1-bit = 12 x 288 bytes = ~3.5KB total. Fits easily in PROGMEM.
- **LED priority**: Agent state LED should override any other LED effect immediately.

---

*Based on EleksCava Hardware Specification v1.0 (firmware v2.0.8)*
*Adapted for InksPet by EleksCava project*
