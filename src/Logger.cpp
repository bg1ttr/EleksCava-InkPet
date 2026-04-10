#include "Logger.h"
#include <stdarg.h>

void Logger::info(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log("INFO", tag, fmt, args);
    va_end(args);
}

void Logger::error(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log("ERROR", tag, fmt, args);
    va_end(args);
}

void Logger::warning(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log("WARN", tag, fmt, args);
    va_end(args);
}

void Logger::debug(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log("DEBUG", tag, fmt, args);
    va_end(args);
}

void Logger::log(const char* level, const char* tag, const char* fmt, va_list args) {
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    Serial.printf("[%s][%s] %s\n", level, tag, buf);
}
