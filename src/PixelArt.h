#pragma once
#include <Arduino.h>

#define PIXELART_WIDTH  48
#define PIXELART_HEIGHT 48

// Get pixel art bitmap for an agent state
const uint8_t* getPixelArtForState(const char* stateName);

// Individual state bitmaps
extern const uint8_t PROGMEM pixelart_sleeping[];
extern const uint8_t PROGMEM pixelart_idle[];
extern const uint8_t PROGMEM pixelart_thinking[];
extern const uint8_t PROGMEM pixelart_working[];
extern const uint8_t PROGMEM pixelart_error[];
extern const uint8_t PROGMEM pixelart_completed[];
extern const uint8_t PROGMEM pixelart_permission[];
extern const uint8_t PROGMEM pixelart_juggling[];
extern const uint8_t PROGMEM pixelart_conducting[];
extern const uint8_t PROGMEM pixelart_sweeping[];
extern const uint8_t PROGMEM pixelart_carrying[];
