#include "RGBLed.h"
#include "config.h"
#include "Logger.h"
#include <math.h>

static const char* TAG = "LED";
RGBLed* RGBLed::_instance = nullptr;

RGBLed::RGBLed()
    : _strip(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800),
      _currentEffect(LedEffect::OFF),
      _currentColor(LedColors::OFF),
      _brightness(50),
      _lastUpdate(0),
      _animStep(0),
      _initialized(false)
{}

RGBLed* RGBLed::getInstance() {
    if (!_instance) _instance = new RGBLed();
    return _instance;
}

void RGBLed::init() {
    _strip.begin();
    _strip.setBrightness(10);  // Low brightness during boot
    _strip.clear();
    _strip.show();
    _initialized = true;
    LOG_INFO(TAG, "RGB LED initialized (%d LEDs)", RGB_LED_COUNT);
}

void RGBLed::setColor(LedColor color) {
    _currentColor = color;
    _currentEffect = LedEffect::SOLID;
    applyColor(color.r, color.g, color.b);
}

void RGBLed::setEffect(LedEffect effect, LedColor color) {
    _currentEffect = effect;
    _currentColor = color;
    _animStep = 0;

    if (effect == LedEffect::OFF) {
        applyColor(0, 0, 0);
    } else if (effect == LedEffect::SOLID) {
        applyColor(color.r, color.g, color.b);
    }
}

void RGBLed::off() {
    _currentEffect = LedEffect::OFF;
    applyColor(0, 0, 0);
}

void RGBLed::setBrightness(uint8_t brightness) {
    _brightness = brightness;
    _strip.setBrightness(brightness);
    _strip.show();
}

void RGBLed::setBrightnessLevel(const String& level) {
    if (level == "off") setBrightness(0);
    else if (level == "low") setBrightness(25);
    else if (level == "medium") setBrightness(128);
    else if (level == "high") setBrightness(255);
}

void RGBLed::startRainbow() {
    _currentEffect = LedEffect::RAINBOW;
    _animStep = 0;
}

void RGBLed::stopAll() {
    off();
}

void RGBLed::update() {
    if (!_initialized) return;

    unsigned long now = millis();

    switch (_currentEffect) {
        case LedEffect::OFF:
            break;

        case LedEffect::SOLID:
            // No animation needed
            break;

        case LedEffect::BREATHING: {
            if (now - _lastUpdate < 20) return;
            _lastUpdate = now;
            _animStep++;

            // Sine wave breathing: period ~2s (100 steps * 20ms)
            float phase = (float)_animStep / 100.0f * 2.0f * PI;
            float factor = (sin(phase) + 1.0f) / 2.0f;  // 0.0 - 1.0
            factor = factor * 0.9f + 0.1f;  // 0.1 - 1.0 (never fully off)

            applyColor(
                (uint8_t)(_currentColor.r * factor),
                (uint8_t)(_currentColor.g * factor),
                (uint8_t)(_currentColor.b * factor)
            );
            break;
        }

        case LedEffect::FLASH: {
            if (now - _lastUpdate < 250) return;
            _lastUpdate = now;
            _animStep++;

            if (_animStep % 2 == 0) {
                applyColor(_currentColor.r, _currentColor.g, _currentColor.b);
            } else {
                applyColor(0, 0, 0);
            }
            break;
        }

        case LedEffect::FAST_FLASH: {
            if (now - _lastUpdate < 100) return;
            _lastUpdate = now;
            _animStep++;

            if (_animStep % 2 == 0) {
                applyColor(_currentColor.r, _currentColor.g, _currentColor.b);
            } else {
                applyColor(0, 0, 0);
            }
            break;
        }

        case LedEffect::FADE_ONCE: {
            if (now - _lastUpdate < 20) return;
            _lastUpdate = now;
            _animStep++;

            if (_animStep <= 50) {
                // Fade in
                float factor = (float)_animStep / 50.0f;
                applyColor(
                    (uint8_t)(_currentColor.r * factor),
                    (uint8_t)(_currentColor.g * factor),
                    (uint8_t)(_currentColor.b * factor)
                );
            } else if (_animStep <= 100) {
                // Fade out
                float factor = 1.0f - (float)(_animStep - 50) / 50.0f;
                applyColor(
                    (uint8_t)(_currentColor.r * factor),
                    (uint8_t)(_currentColor.g * factor),
                    (uint8_t)(_currentColor.b * factor)
                );
            } else {
                _currentEffect = LedEffect::OFF;
                applyColor(0, 0, 0);
            }
            break;
        }

        case LedEffect::RAINBOW: {
            if (now - _lastUpdate < 30) return;
            _lastUpdate = now;
            _animStep++;

            for (int i = 0; i < RGB_LED_COUNT; i++) {
                uint16_t hue = ((_animStep * 256 / 50) + (i * 65536 / RGB_LED_COUNT)) & 0xFFFF;
                uint32_t color = _strip.ColorHSV(hue, 255, 200);
                _strip.setPixelColor(i, color);
            }
            _strip.show();
            break;
        }
    }
}

void RGBLed::applyColor(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < RGB_LED_COUNT; i++) {
        _strip.setPixelColor(i, _strip.Color(r, g, b));
    }
    _strip.show();
}
