#include "KeyManager.h"
#include "config.h"
#include "Logger.h"

static const char* TAG = "Keys";
KeyManager* KeyManager::_instance = nullptr;

KeyManager::KeyManager() : _lastDebounce(0) {
    _btnA = {KEY_A, true, true, 0, false};
    _btnB = {KEY_B, true, true, 0, false};
    _btnC = {KEY_C, true, true, 0, false};
}

KeyManager* KeyManager::getInstance() {
    if (!_instance) _instance = new KeyManager();
    return _instance;
}

void KeyManager::init() {
    pinMode(KEY_A, INPUT_PULLUP);
    pinMode(KEY_B, INPUT_PULLUP);
    pinMode(KEY_C, INPUT_PULLUP);
    delay(10);  // Let pull-ups stabilize

    // Read actual pin states after init to prevent phantom press on boot
    bool aState = digitalRead(KEY_A) == LOW;
    bool bState = digitalRead(KEY_B) == LOW;
    bool cState = digitalRead(KEY_C) == LOW;
    _btnA = {KEY_A, aState, aState, 0, false};
    _btnB = {KEY_B, bState, bState, 0, false};
    _btnC = {KEY_C, cState, cState, 0, false};

    // Set debounce to now so first poll doesn't trigger immediately
    _lastDebounce = millis();

    LOG_INFO(TAG, "Buttons initialized (A=%d, B=%d, C=%d)", KEY_A, KEY_B, KEY_C);
}

bool KeyManager::isComboPressed() const {
    return (digitalRead(KEY_A) == LOW && digitalRead(KEY_C) == LOW);
}

KeyEvent KeyManager::checkButton(ButtonState& btn, KeyEvent shortEvt, KeyEvent longEvt) {
    bool reading = digitalRead(btn.pin) == LOW;  // Active low

    if (reading != btn.lastState) {
        _lastDebounce = millis();
    }

    if (millis() - _lastDebounce > DEBOUNCE_DELAY_MS) {
        if (reading != btn.currentState) {
            btn.currentState = reading;

            if (btn.currentState) {
                // Just pressed
                btn.pressTime = millis();
                btn.longFired = false;
            } else {
                // Just released
                if (!btn.longFired) {
                    btn.lastState = reading;
                    return shortEvt;
                }
            }
        }

        // Check for long press while held
        if (btn.currentState && !btn.longFired) {
            if (millis() - btn.pressTime > LONG_PRESS_MS) {
                btn.longFired = true;
                btn.lastState = reading;
                return longEvt;
            }
        }
    }

    btn.lastState = reading;
    return KeyEvent::NONE;
}

KeyEvent KeyManager::poll() {
    // Check combo first
    if (isComboPressed()) {
        static unsigned long comboStart = 0;
        if (comboStart == 0) comboStart = millis();
        if (millis() - comboStart > FACTORY_RESET_COMBO_MS) {
            comboStart = 0;
            LOG_INFO(TAG, "A+C combo detected - AP mode entry");
            return KeyEvent::COMBO_AC;
        }
    } else {
        // Reset combo timer when released
    }

    KeyEvent evt;

    evt = checkButton(_btnA, KeyEvent::KEY_A_SHORT, KeyEvent::KEY_A_LONG);
    if (evt != KeyEvent::NONE) return evt;

    evt = checkButton(_btnB, KeyEvent::KEY_B_SHORT, KeyEvent::KEY_B_LONG);
    if (evt != KeyEvent::NONE) return evt;

    evt = checkButton(_btnC, KeyEvent::KEY_C_SHORT, KeyEvent::KEY_C_LONG);
    if (evt != KeyEvent::NONE) return evt;

    return KeyEvent::NONE;
}
