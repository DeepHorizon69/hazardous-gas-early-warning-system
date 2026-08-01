# AI Context & Orientation Guide

This document provides context for AI coding assistants working on this codebase.

---

## Codebase Summary

- **Repository**: Hazardous Gas Early Warning System (ESP32)
- **Language**: C++17 / Arduino / FreeRTOS framework
- **Main Entry**: `dax.ino`
- **Core Architecture**: Dual-core FreeRTOS application:
  - `runSensorSafetyTask` (Core 1, Priority 5, 100ms cycle): High-priority safety execution path (Sensors -> DSP -> State Engine -> Alarms -> Display).
  - `runCommunicationTask` (Core 0, Priority 1, 50ms loop): Low-priority network communication path (WiFi -> MQTT telemetry & remote config).

---

## Critical Files & Roles

| File | Purpose |
| --- | --- |
| `Config.h` | System constants, pin definitions, state enum, and `DeviceConfig` struct. |
| `Logger.h` | Thread-safe logging macros (`LOGI`, `LOGW`, `LOGE`, `LOGD`). |
| `Filters.h` | Header-only DSP header (`MedianFilter`, `ExponentialMovingAverage`). |
| `SensorManager.h/.cpp` | Sensor sampling (MQ-2, MQ-4, MQ-135, DHT22), ADC fault checks, temp/hum compensation. |
| `SystemManager.h/.cpp` | Safety state machine, hysteresis latching, rate-of-rise (RoR) spike evaluation. |
| `AlarmManager.h/.cpp` | Non-blocking piezo buzzer pulse generator. |
| `DisplayManager.h/.cpp` | 128x64 SSD1306 OLED display driver & page manager with I2C bus auto-recovery. |
| `NetworkManager.h/.cpp` | WiFi connection & exponential backoff manager. |
| `MQTTManager.h/.cpp` | PubSubClient wrapper for telemetry publish, LWT state, and remote configuration callback. |
| `StorageManager.h/.cpp` | NVS configuration reader/writer using ESP32 `Preferences`. |

---

## Key Anti-Patterns To Avoid

- **Do NOT introduce `std::function` or dynamic heap allocation (`new`/`malloc`/`String`) inside loop execution paths.**
- **Do NOT move safety processing to Core 0 or merge it with network tasks.**
- **Do NOT add network dependencies to `SensorManager`, `SystemManager`, or `AlarmManager`.**
- **Do NOT alter NVS key strings (`mq2_en`, `mq2_on`, etc.) or MQTT payload schema keys.**
