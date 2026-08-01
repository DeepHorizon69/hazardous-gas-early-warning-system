# Development Guide - Hazardous Gas Early Warning System

This guide outlines setup instructions, build configurations, debugging standards, and coding conventions for maintaining the codebase.

---

## 1. Prerequisites & Environment Setup

### Tools Required
- **Arduino IDE** (v2.0+) OR **VS Code** with **PlatformIO Extension**.
- **Espressif ESP32 Board Package** (v2.0.11 or later).
- Serial Terminal (115200 baud).

### Dependencies
Ensure the following libraries are installed:
- `Adafruit SSD1306`
- `Adafruit GFX Library`
- `DHT sensor library`
- `PubSubClient`
- `ArduinoJson`

---

## 2. Compilation & Building

### Target Board
- **Board**: `ESP32 Dev Module`
- **Flash Frequency**: `80MHz`
- **Partition Scheme**: `Default 4MB with spiffs` or `Huge APP (3MB No OTA)`
- **Core Debug Level**: `None` (or `Info` during development)

### Preprocessor Flags
- Pass `-DDEBUG_BUILD` during compilation to enable debug level logging (`LOGD`).

---

## 3. Logging & Diagnostics

Logging is provided by `Logger.h` using standard macros:

```cpp
LOGI(TAG, "Informational message: %d", value);
LOGW(TAG, "Warning message: %s", reason);
LOGE(TAG, "Error message: %s", errDetail);
LOGD(TAG, "Debug details"); // Active only when DEBUG_BUILD is defined
```

Outputs are prefixed with system uptime in milliseconds:
`[10245][INFO][SensorManager] SensorManager initialized`

---

## 4. Coding Conventions

- **Headers**: Use `#pragma once` for all header files.
- **Namespaces**: Keep system constants and structures inside `namespace Config`.
- **Member Variables**: Prefix class member variables with `m_` (e.g. `m_buzzerState`, `m_lastDraw`).
- **Memory**: Never call `malloc` or `new` in execution loops. Allocate on stack or as member variables.
- **Function Pointers**: Use standard C function pointers (`void (*ConfigCallback)(...)`) rather than `std::function`.
