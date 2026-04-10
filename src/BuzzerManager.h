#pragma once
#include <Arduino.h>

struct Melody {
    const uint16_t* frequencies;
    const uint16_t* durations;
    uint8_t noteCount;
};

class BuzzerManager {
public:
    static BuzzerManager* getInstance();
    void init();

    void playTone(uint16_t freq, uint16_t durationMs);
    void playMelody(const Melody& melody);
    void playPermissionAlert();
    void playTaskComplete();
    void playError();
    void playConnect();
    void playWelcome();
    void stop();

    void setEnabled(bool enabled) { _enabled = enabled; }
    bool isEnabled() const { return _enabled; }
    void setVolume(uint8_t duty) { _volume = duty; }

    void update();  // Call from loop for async playback

private:
    BuzzerManager();
    static BuzzerManager* _instance;

    bool _enabled;
    uint8_t _volume;  // PWM duty (0-255)

    // Async playback state
    const uint16_t* _melFreqs;
    const uint16_t* _melDurs;
    uint8_t _melCount;
    uint8_t _melIndex;
    unsigned long _melNoteStart;
    bool _playing;
};
