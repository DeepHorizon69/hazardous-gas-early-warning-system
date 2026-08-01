# Architecture Specification - Hazardous Gas Early Warning System

This document describes the architectural principles, task boundaries, data flow, and concurrency model of the Hazardous Gas Early Warning System firmware.

---

## 1. Architectural Principles

1. **Safety Decoupling**: Safety-critical hardware control (sampling, state machine, buzzer, display) operates on **Core 1** with high FreeRTOS priority (5). Communication tasks (WiFi, MQTT) operate on **Core 0** with lower priority (1). Loss of network connectivity **never** impairs sensor sampling or hazard warning activation.
2. **Zero Hot-Path Allocation**: Sensor sampling, filter execution, state evaluation, display updating, and buzzer driving use statically allocated buffers and stack variables. Heap allocation is prohibited inside loop paths.
3. **Transport Independence**: Business logic (`SystemManager`) operates strictly on numerical values and boolean flags. It has zero dependency on MQTT, WiFi, or OLED hardware libraries.
4. **Driver Insulation**: Hardware drivers (`DHT`, `Adafruit_SSD1306`, `PubSubClient`, `Preferences`) are encapsulated within manager classes (`SensorManager`, `DisplayManager`, `MQTTManager`, `StorageManager`).

---

## 2. Multi-Core FreeRTOS Task Layout

```
                  +-----------------------------------+
                  |           ESP32 MCU               |
                  +-----------------+-----------------+
                                    |
           +------------------------+------------------------+
           |                                                 |
    +------v------+                                   +------v------+
    |   CORE 1    |                                   |   CORE 0    |
    | (Priority 5)|                                   | (Priority 1)|
    +------+------+                                   +------+------+
           |                                                 |
  [runSensorSafetyTask]                             [runCommunicationTask]
     (10Hz / 100ms)                                     (50ms yield)
           |                                                 |
  +--------+--------+                               +--------+--------+
  | - SensorManager |                               | - NetworkManager|
  | - SystemManager |                               | - MQTTManager   |
  | - AlarmManager  |                               +-----------------+
  | - DisplayManager|                                        ^
  +--------+--------+                                        |
           |                                                 |
           +-------------> [configMutex] <-------------------+
                           (FreeRTOS Mutex)
```

---

## 3. Inter-Task Communication & Synchronization

- **`configMutex`**: Protects concurrent access to `activeConfig` (`Config::DeviceConfig`).
- **`sharedSystemState`**: Volatile variable updated by `runSensorSafetyTask` and read by `runCommunicationTask` for state change publishing.
- **`hasStatePendingPublish`**: Volatile flag set when a state transition occurs to trigger instant event-driven MQTT state publishing.

---

## 4. Module Dependencies

```
[dax.ino]
  ├──> [Config]
  ├──> [Logger]
  ├──> [StorageManager]
  ├──> [SensorManager] ──> [Filters]
  ├──> [SystemManager]
  ├──> [AlarmManager]
  ├──> [DisplayManager]
  ├──> [NetworkManager]
  └──> [MQTTManager]
```

Notice that `SensorManager`, `SystemManager`, `AlarmManager`, and `DisplayManager` have **zero** dependencies on `NetworkManager` or `MQTTManager`.
