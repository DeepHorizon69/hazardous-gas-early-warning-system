#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class Logger {
public:
    static void init() {
        if (m_mutex == nullptr) {
            m_mutex = xSemaphoreCreateMutex();
        }
    }

    static void log(const char* level, const char* tag, const char* format, ...) {
        if (m_mutex == nullptr) return;

        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            Serial.printf("[%lu][%s][%s] %s\n", millis(), level, tag, buffer);
            xSemaphoreGive(m_mutex);
        }
    }

    static void info(const char* tag, const char* format, ...) {
        va_list args;
        va_start(args, format);
        log("INFO", tag, format, args);
        va_end(args);
    }

    static void warn(const char* tag, const char* format, ...) {
        va_list args;
        va_start(args, format);
        log("WARN", tag, format, args);
        va_end(args);
    }

    static void error(const char* tag, const char* format, ...) {
        va_list args;
        va_start(args, format);
        log("ERROR", tag, format, args);
        va_end(args);
    }

    static void debug(const char* tag, const char* format, ...) {
#ifdef DEBUG_BUILD
        va_list args;
        va_start(args, format);
        log("DEBUG", tag, format, args);
        va_end(args);
#endif
    }

private:
    static SemaphoreHandle_t m_mutex;
};

// Define the static member in a cpp file or keep it inline/external
inline SemaphoreHandle_t Logger::m_mutex = nullptr;
#define LOGI(tag, fmt, ...) Logger::log("INFO", tag, fmt, ##__VA_ARGS__)
#define LOGW(tag, fmt, ...) Logger::log("WARN", tag, fmt, ##__VA_ARGS__)
#define LOGE(tag, fmt, ...) Logger::log("ERROR", tag, fmt, ##__VA_ARGS__)
#define LOGD(tag, fmt, ...) Logger::log("DEBUG", tag, fmt, ##__VA_ARGS__)
