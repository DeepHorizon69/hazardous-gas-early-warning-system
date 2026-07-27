#pragma once

#include <Arduino.h>

namespace Config {
    // Hardware Pin Mappings
    constexpr uint8_t PIN_MQ2 = 34;
    constexpr uint8_t PIN_MQ4 = 35;
    constexpr uint8_t PIN_MQ135 = 32;
    constexpr uint8_t PIN_DHT22 = 4;
    constexpr uint8_t PIN_BUZZER = 26;
    constexpr uint8_t PIN_I2C_SDA = 21;
    constexpr uint8_t PIN_I2C_SCL = 22;

    // Display Constants
    constexpr bool OLED_ENABLED = true;
    constexpr uint16_t SCREEN_WIDTH = 128;
    constexpr uint16_t SCREEN_HEIGHT = 64;
    constexpr uint8_t OLED_ADDRESS = 0x3C;

    // Timing Constants (Milliseconds)
    constexpr uint32_t HEATING_DURATION_MS = 300000UL; // 5 minutes
    constexpr uint32_t TELEMETRY_INTERVAL_MS = 2000UL;
    constexpr uint32_t OLED_INTERVAL_MS = 500UL;
    constexpr uint32_t FAULT_CHECK_INTERVAL_MS = 1000UL;
    constexpr uint32_t MONITOR_PAGE_TIME_MS = 5000UL;
    constexpr uint32_t NETWORK_PAGE_TIME_MS = 3000UL;
    constexpr uint32_t SENSOR_READ_INTERVAL_MS = 100UL; // 10Hz sampling

    // Reconnection Limits
    constexpr uint32_t WIFI_RECONNECT_MIN_MS = 2000UL;
    constexpr uint32_t WIFI_RECONNECT_MAX_MS = 60000UL;
    constexpr uint32_t MQTT_RECONNECT_MIN_MS = 2000UL;
    constexpr uint32_t MQTT_RECONNECT_MAX_MS = 60000UL;

    // Calibration & Compensation Defaults
    constexpr float TEMP_COMP_COEFF = -0.005f; // Correction per °C from 20°C
    constexpr float HUM_COMP_COEFF = -0.002f;  // Correction per %RH from 65%RH

    // Safety Algorithms
    constexpr uint16_t HYSTERESIS_VAL = 100;
    constexpr float RATE_OF_RISE_THRESHOLD = 50.0f; // Units per second change

    // Network Config
    extern const char* WIFI_SSID;
    extern const char* WIFI_PASSWORD;
    extern const char* MQTT_SERVER;
    extern uint16_t MQTT_PORT;
    extern const char* MQTT_NAMESPACE;

    // Preferences (NVS) Namespace
    constexpr const char* NVS_NAMESPACE = "gascfg";

    // System States
    enum class SystemState : uint8_t {
        BOOT = 0,
        HEATING,
        SAFE,
        NORMAL,
        MODERATE,
        WARNING,
        DANGER,
        CRITICAL,
        FAULT
    };

    inline const char* getStateText(SystemState state) {
        switch (state) {
            case SystemState::BOOT:     return "BOOT";
            case SystemState::HEATING:  return "HEATING";
            case SystemState::SAFE:     return "SAFE";
            case SystemState::NORMAL:   return "NORMAL";
            case SystemState::MODERATE: return "MODERATE";
            case SystemState::WARNING:  return "WARNING";
            case SystemState::DANGER:   return "DANGER";
            case SystemState::CRITICAL: return "CRITICAL";
            case SystemState::FAULT:    return "FAULT";
            default:                    return "UNKNOWN";
        }
    }

    // Config Structure
    struct DeviceConfig {
        bool mq2Enabled;
        int mq2On;
        int mq2Off;

        bool mq4Enabled;
        int mq4On;
        int mq4Off;

        bool mq135Enabled;
        int mq135On;
        int mq135Off;

        bool dhtEnabled;
        float tempWarn;

        bool buzzerEnabled;
    };
}
