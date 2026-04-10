#include "BuzzerManager.h"
#include "config.h"
#include "Logger.h"

static const char* TAG = "Buzzer";
BuzzerManager* BuzzerManager::_instance = nullptr;

// Melodies stored in PROGMEM
static const uint16_t MELODY_PERMISSION_FREQ[] = {1500, 1500, 800};
static const uint16_t MELODY_PERMISSION_DUR[] = {80, 80, 200};

static const uint16_t MELODY_COMPLETE_FREQ[] = {523, 659, 784};
static const uint16_t MELODY_COMPLETE_DUR[] = {120, 120, 200};

static const uint16_t MELODY_ERROR_FREQ[] = {400, 300};
static const uint16_t MELODY_ERROR_DUR[] = {200, 400};

static const uint16_t MELODY_CONNECT_FREQ[] = {1000};
static const uint16_t MELODY_CONNECT_DUR[] = {80};

static const uint16_t MELODY_WELCOME_FREQ[] = {523, 659, 784, 1047};
static const uint16_t MELODY_WELCOME_DUR[] = {100, 100, 100, 200};

BuzzerManager::BuzzerManager()
    : _enabled(true), _volume(64),
      _melFreqs(nullptr), _melDurs(nullptr), _melCount(0),
      _melIndex(0), _melNoteStart(0), _playing(false)
{}

BuzzerManager* BuzzerManager::getInstance() {
    if (!_instance) _instance = new BuzzerManager();
    return _instance;
}

void BuzzerManager::init() {
    ledcSetup(BUZZER_CHANNEL, 5000, 8);
    ledcAttachPin(PIN_BUZZER, BUZZER_CHANNEL);
    ledcWriteTone(BUZZER_CHANNEL, 0);
    LOG_INFO(TAG, "Buzzer initialized on GPIO %d", PIN_BUZZER);
}

void BuzzerManager::playTone(uint16_t freq, uint16_t durationMs) {
    if (!_enabled) return;
    ledcWriteTone(BUZZER_CHANNEL, freq);
    ledcWrite(BUZZER_CHANNEL, _volume);
    delay(durationMs);
    ledcWriteTone(BUZZER_CHANNEL, 0);
}

void BuzzerManager::playMelody(const Melody& melody) {
    if (!_enabled) return;
    _melFreqs = melody.frequencies;
    _melDurs = melody.durations;
    _melCount = melody.noteCount;
    _melIndex = 0;
    _melNoteStart = millis();
    _playing = true;

    // Start first note
    ledcWriteTone(BUZZER_CHANNEL, _melFreqs[0]);
    ledcWrite(BUZZER_CHANNEL, _volume);
}

void BuzzerManager::playPermissionAlert() {
    Melody m = {MELODY_PERMISSION_FREQ, MELODY_PERMISSION_DUR, 3};
    playMelody(m);
}

void BuzzerManager::playTaskComplete() {
    Melody m = {MELODY_COMPLETE_FREQ, MELODY_COMPLETE_DUR, 3};
    playMelody(m);
}

void BuzzerManager::playError() {
    Melody m = {MELODY_ERROR_FREQ, MELODY_ERROR_DUR, 2};
    playMelody(m);
}

void BuzzerManager::playConnect() {
    Melody m = {MELODY_CONNECT_FREQ, MELODY_CONNECT_DUR, 1};
    playMelody(m);
}

void BuzzerManager::playWelcome() {
    Melody m = {MELODY_WELCOME_FREQ, MELODY_WELCOME_DUR, 4};
    playMelody(m);
}

void BuzzerManager::stop() {
    ledcWriteTone(BUZZER_CHANNEL, 0);
    _playing = false;
}

void BuzzerManager::update() {
    if (!_playing || !_enabled) return;

    if (millis() - _melNoteStart >= _melDurs[_melIndex]) {
        // Short gap between notes
        ledcWriteTone(BUZZER_CHANNEL, 0);

        _melIndex++;
        if (_melIndex >= _melCount) {
            _playing = false;
            return;
        }

        // Start next note after tiny gap
        _melNoteStart = millis() + 30;  // 30ms gap
        ledcWriteTone(BUZZER_CHANNEL, _melFreqs[_melIndex]);
        ledcWrite(BUZZER_CHANNEL, _volume);
        _melNoteStart = millis();
    }
}
