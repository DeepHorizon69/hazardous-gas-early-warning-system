#pragma once

#include <Arduino.h>
#include <cstdarg>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class Logger {
public:
    static void init() {
        if (m_mutex == nullptr) {
            m_mutex = xSemaphoreCreateMutex();
        }
    }

    static void logv(const char* level, const char* tag, const char* format, va_list args) {
        if (m_mutex == nullptr) return;

        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);

        if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            Serial.printf("[%lu][%s][%s] %s\n", millis(), level, tag, buffer);
            xSemaphoreGive(m_mutex);
        }
    }

    static void log(const char* level, const char* tag, const char* format, ...) {
        va_list args;
        va_start(args, format);
        logv(level, tag, format, args);
        va_end(args);
    }

    static void info(const char* tag, const char* format, ...) {
        va_list args;
        va_start(args, format);
        logv("INFO", tag, format, args);
        va_end(args);
    }

    static void warn(const char* tag, const char* format, ...) {
        va_list args;
        va_start(args, format);
        logv("WARN", tag, format, args);
        va_end(args);
    }

    static void error(const char* tag, const char* format, ...) {
        va_list args;
        va_start(args, format);
        logv("ERROR", tag, format, args);
        va_end(args);
    }

    static void debug(const char* tag, const char* format, ...) {
#ifdef DEBUG_BUILD
        va_list args;
        va_start(args, format);
        logv("DEBUG", tag, format, args);
        va_end(args);
#endif
    }

private:
    inline static SemaphoreHandle_t m_mutex = nullptr;
};

#define LOGI(tag, fmt, ...) Logger::log("INFO", tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) Logger::log("WARN", tag, fmt, ##__VA_ARGS__)
#define LOGE(tag, fmt, ...) Logger::log("ERROR", tag, fmt, ##__VA_ARGS__)
#define LOGD(tag, fmt, ...) Logger::log("DEBUG", tag, fmt, ##__VA_ARGS__)
