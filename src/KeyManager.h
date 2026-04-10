#pragma once
#include <Arduino.h>
#include <functional>

enum class KeyEvent {
    NONE,
    KEY_A_SHORT,
    KEY_B_SHORT,
    KEY_C_SHORT,
    KEY_A_LONG,
    KEY_B_LONG,
    KEY_C_LONG,
    COMBO_AC     // Factory reset / AP mode entry
};

class KeyManager {
public:
    static KeyManager* getInstance();
    void init();
    KeyEvent poll();  // Returns key event, call from loop
    bool isComboPressed() const;

private:
    KeyManager();
    static KeyManager* _instance;

    struct ButtonState {
        uint8_t pin;
        bool lastState;
        bool currentState;
        unsigned long pressTime;
        bool longFired;
    };

    ButtonState _btnA, _btnB, _btnC;
    unsigned long _lastDebounce;

    KeyEvent checkButton(ButtonState& btn, KeyEvent shortEvt, KeyEvent longEvt);
};
