#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

enum class LedEffect {
    OFF,
    SOLID,
    BREATHING,
    FLASH,
    FAST_FLASH,
    FADE_ONCE,
    RAINBOW
};

struct LedColor {
    uint8_t r, g, b;
};

// Predefined colors for agent states
namespace LedColors {
    constexpr LedColor OFF       = {0, 0, 0};
    constexpr LedColor WHITE_DIM = {30, 30, 30};
    constexpr LedColor BLUE      = {0, 80, 255};
    constexpr LedColor GREEN     = {0, 200, 0};
    constexpr LedColor RED       = {255, 0, 0};
    constexpr LedColor YELLOW    = {255, 180, 0};
    constexpr LedColor PURPLE    = {160, 0, 255};
    constexpr LedColor CYAN      = {0, 200, 200};
    constexpr LedColor ORANGE    = {255, 100, 0};
    constexpr LedColor WHITE     = {200, 200, 200};
}

class RGBLed {
public:
    static RGBLed* getInstance();
    void init();

    void setColor(LedColor color);
    void setEffect(LedEffect effect, LedColor color);
    void off();
    void setBrightness(uint8_t brightness);
    void setBrightnessLevel(const String& level);
    void startRainbow();
    void stopAll();

    void update();  // Call from loop for animation

private:
    RGBLed();
    static RGBLed* _instance;

    Adafruit_NeoPixel _strip;
    LedEffect _currentEffect;
    LedColor _currentColor;
    uint8_t _brightness;
    unsigned long _lastUpdate;
    uint16_t _animStep;
    bool _initialized;

    void applyColor(uint8_t r, uint8_t g, uint8_t b);
    uint8_t breathe(uint8_t val) const;
};
