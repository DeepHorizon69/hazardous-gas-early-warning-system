# Hazardous Gas Early Warning System (ESP32)

An industrial-grade, multi-sensor hazardous gas detection and environmental monitoring firmware for ESP32. The system acquires telemetry from gas sensors (MQ-2, MQ-4, MQ-135) and environmental sensors (DHT22), performs DSP filtering (Median + EMA), evaluates multi-tier hazard thresholds with hysteresis and rate-of-rise (RoR) detection, and drives local OLED visual diagnostics, audio alarms, and real-time MQTT telemetry reporting.

Online Dashboard: [https://deephorizon69.github.io/iot-dashboard/](https://deephorizon69.github.io/iot-dashboard/)

---

## Technical Overview

- **Core Microcontroller**: ESP32 (Xtensa Dual-Core 32-bit LX6)
- **Multi-Task Architecture**: FreeRTOS dual-core task isolation:
  - **Core 1 (High Priority 5, 10Hz / 100ms cycle)**: Safety-critical sensor acquisition, DSP filtering, state evaluation, buzzer alarms, and OLED display rendering.
  - **Core 0 (Low Priority 1, 50ms sleep loop)**: WiFi non-blocking reconnection, MQTT telemetry, LWT state publishing, and remote configuration handling.
- **Sensory Peripherals**:
  - **MQ-2**: Combustible gas & LPG detection (ADC Pin 34)
  - **MQ-4**: Methane & CNG detection (ADC Pin 35)
  - **MQ-135**: Air quality & toxic gas detection (ADC Pin 32)
  - **DHT22**: Temperature & Relative Humidity sensor (GPIO 4)
- **Actuators & Visuals**:
  - **Piezo Buzzer**: Active alert patterns (GPIO 26)
  - **OLED Display**: 128x64 SSD1306 via I2C (SDA 21, SCL 22, Address `0x3C`)
- **Storage**: Non-Volatile Storage (NVS via ESP32 `Preferences`) for persistent configuration across reboots.

---

## Directory Structure

```
hazardous-gas-early-warning-system/
├── dax.ino              # Main application entry point & FreeRTOS task scheduling
├── Config.h             # Pin mappings, system constants, state enum, and structs
├── Logger.h             # Thread-safe variadic logging macros (LOGI, LOGW, LOGE, LOGD)
├── Filters.h            # DSP filters (MedianFilter, ExponentialMovingAverage)
├── AlarmManager.h/.cpp  # Non-blocking audio alarm pattern controller
├── DisplayManager.h/.cpp# OLED UI rendering, page rotation, and I2C recovery
├── NetworkManager.h/.cpp# Non-blocking WiFi reconnect state machine
├── MQTTManager.h/.cpp   # MQTT telemetry, LWT, and remote configuration client
├── SensorManager.h/.cpp # Sensor sampling, ADC bounds checking, & compensation
├── StorageManager.h/.cpp# NVS configuration storage engine
├── SystemManager.h/.cpp # Hazard detection state machine, hysteresis & RoR calculation
├── ARCHITECTURE.md      # Detailed system architecture document
├── DEVELOPMENT_GUIDE.md # Setup, compilation, flashing, and debugging guide
├── EXTENSION_GUIDE.md   # Guide for adding new sensors, outputs, or transports
├── PROJECT_RULES.md     # Mandatory engineering standards and constraints
├── CONTRIBUTING.md      # Contribution guidelines for developers
├── AI_CONTEXT.md        # Technical summary for AI coding assistants
└── AGENTS.md            # Operational instructions for automated subagents
```

---

## System States

The safety state engine evaluates inputs every 100ms and transitions through the following hierarchy:

| State | Condition | Buzzer Behavior |
| --- | --- | --- |
| `BOOT` | System initialization | Silent |
| `HEATING` | 5-minute sensor element pre-heating warm-up phase | Silent |
| `FAULT` | Sensor disconnected or ADC hardware fault detected | Intermittent short blip (100ms ON / 2500ms OFF) |
| `CRITICAL` | 2+ sensors in Danger state OR Temperature $\ge 50^\circ\text{C}$ | Continuous tone (100% ON) |
| `DANGER` | Any sensor exceeds Danger threshold (`mqXOn`) | Continuous tone (100% ON) |
| `WARNING` | Any sensor exceeds Warning threshold (`mqXOff`) OR Temp $\ge \text{tempWarn}$ | Pulsed tone (200ms ON / 800ms OFF) |
| `MODERATE` | Rate of Rise (RoR) spike detected ($\ge 50.0$ units/sec) | Intermittent blip |
| `SAFE` | Readings below Warning threshold by `SAFE_MARGIN` (300) | Silent |
| `NORMAL` | Normal baseline monitoring | Silent |

---

## Build & Flashing Instructions

### Requirements
- Arduino IDE 2.x or PlatformIO
- ESP32 Board Support Package (`esp32` by Espressif Systems v2.0+)
- Required Libraries:
  - `Adafruit_SSD1306`
  - `Adafruit_GFX`
  - `DHT sensor library`
  - `PubSubClient`
  - `ArduinoJson` (v6 or v7)

### Flashing via Arduino IDE
1. Open `dax.ino` in Arduino IDE.
2. Select target board: **ESP32 Dev Module**.
3. Set CPU Frequency: **240MHz**.
4. Set Flash Frequency: **80MHz**.
5. Connect ESP32 via USB and select the correct COM port.
6. Click **Upload**.

---

## MQTT Configuration & Protocol

- **Broker**: `broker.emqx.io:1883` (Default namespace: `alsa`)
- **Topics**:
  - `alsa/gas/data` (Publish): Periodic telemetry payload (2s interval).
  - `alsa/gas/state` (Publish): Retained system state updates (`SAFE`, `DANGER`, `CRITICAL`, etc.).
  - `alsa/gas/config` (Subscribe): Remote threshold configuration update JSON.
  - `alsa/gas/config/ack` (Publish): ACK confirmation payload after applying and saving NVS config.

---

## License

Production Firmware — All rights reserved.
