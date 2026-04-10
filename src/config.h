#pragma once

// ============================================================
// InksPet Hardware Pin Definitions
// Based on EleksCava Gen2 hardware platform
// ============================================================

// --- E-Paper Display (SPI) ---
#define EPD_SCK     18
#define EPD_MOSI    23
#define EPD_CS      26
#define EPD_RST     14
#define EPD_DC      27
#define EPD_BUSY    12

// --- Display Dimensions ---
#define DISP_WIDTH  296
#define DISP_HEIGHT 128

// --- Buttons (active LOW, internal pull-up) ---
#define KEY_A       33   // Left button
#define KEY_B       13   // Middle button
#define KEY_C       5    // Right button

// --- RGB LED (WS2812B) ---
#define RGB_LED_PIN     16
#define RGB_LED_COUNT   5
#define RGB_COLOR_ORDER NEO_GRB

// --- Buzzer ---
#define PIN_BUZZER      25
#define BUZZER_CHANNEL  0

// --- Battery ---
#define BATT_ADC_PIN    32
#define USB_STATUS_PIN  17
#define VOLTAGE_DIVIDER_RATIO 2.21f
#define BATT_MIN_VOLTAGE 3.0f
#define BATT_MAX_VOLTAGE 4.2f

// --- Timing Constants ---
#define DEBOUNCE_DELAY_MS       50
#define LONG_PRESS_MS           1000
#define FACTORY_RESET_COMBO_MS  10000

#define PARTIAL_REFRESH_COOLDOWN_MS  800
#define FULL_REFRESH_COOLDOWN_MS     3000
#define FULL_REFRESH_INTERVAL        30    // Force full refresh every N partials

#define SLEEP_TIMEOUT_MS        60000     // 60s idle -> sleeping state
#define PERMISSION_TIMEOUT_MS   60000     // 60s auto-deny
#define WATCHDOG_TIMEOUT_S      180       // 3 minutes

// --- Memory Thresholds ---
#define HEAP_CRITICAL       40000
#define HEAP_WARNING        80000
#define HEAP_MIN_WEBSERVER  65536
#define HEAP_MIN_SSL        40960

// --- Network ---
#define WEBSERVER_PORT      80
#define MDNS_HOSTNAME       "inkspet"
#define AP_SSID_PREFIX      "InksPet_"

// --- NVS Namespaces ---
#define NVS_NAMESPACE_WIFI    "wifi"
#define NVS_NAMESPACE_CONFIG  "inkspet"

// --- Agent State Constants ---
#define MAX_AGENT_SESSIONS  8
#define MAX_PERMISSION_QUEUE 4
