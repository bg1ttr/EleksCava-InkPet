#pragma once
#include <Arduino.h>

#define LOG_INFO(tag, fmt, ...)    Logger::info(tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...)   Logger::error(tag, fmt, ##__VA_ARGS__)
#define LOG_WARNING(tag, fmt, ...) Logger::warning(tag, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(tag, fmt, ...)   Logger::debug(tag, fmt, ##__VA_ARGS__)

class Logger {
public:
    static void info(const char* tag, const char* fmt, ...);
    static void error(const char* tag, const char* fmt, ...);
    static void warning(const char* tag, const char* fmt, ...);
    static void debug(const char* tag, const char* fmt, ...);
private:
    static void log(const char* level, const char* tag, const char* fmt, va_list args);
};
