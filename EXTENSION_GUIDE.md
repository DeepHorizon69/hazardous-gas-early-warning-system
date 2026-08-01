# Extension Guide - Adding Features & Modules

This document provides step-by-step guidance for extending the firmware with new hardware, sensors, outputs, or communication interfaces.

---

## 1. Adding a New Gas Sensor

To add a new analog gas sensor (e.g. MQ-7 for Carbon Monoxide):

1. **`Config.h`**:
   - Define pin mapping: `constexpr uint8_t PIN_MQ7 = 33;`
   - Add threshold fields to `DeviceConfig` struct: `bool mq7Enabled; int mq7On; int mq7Off;`
2. **`SensorManager.h / .cpp`**:
   - Add raw & filtered member variables (`m_mq7Raw`, `m_mq7Filtered`, `m_mq7Median`, `m_mq7Ema`).
   - Update `readMQSensors` and `diagnoseFaults`.
3. **`SystemManager.h / .cpp`**:
   - Add warning/danger latches (`m_mq7WarningLatch`, `m_mq7DangerLatch`).
   - Incorporate into state evaluation logic inside `evaluate()`.
4. **`StorageManager.cpp`**:
   - Add NVS keys `mq7_en`, `mq7_on`, `mq7_off` in `loadConfig()` and `saveConfig()`.
5. **`MQTTManager.cpp`**:
   - Include `"mq7"` in telemetry JSON payload.

---

## 2. Adding Relay Actuator Outputs

To trigger hardware relays (e.g., solenoid exhaust fan valve):

1. **`Config.h`**:
   - Add relay pin mapping: `constexpr uint8_t PIN_RELAY_EXHAUST = 27;`
2. **`ActuatorManager` (New File)**:
   - Create class managing relay outputs based on `SystemState`.
   - Call `actuators.update(state)` inside `runSensorSafetyTask` on Core 1.

---

## 3. Supporting Alternative Connectivity (GSM / LoRa / Ethernet)

Because networking is isolated on **Core 0**:
1. Create new transport manager class (e.g. `LoRaManager`).
2. Instantiate in `dax.ino` and run inside `runCommunicationTask`.
3. Read `sharedSystemState` and sensor readings to construct transport payloads.
