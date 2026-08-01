# Instructions for Automated Subagents

This file contains instructions for AI subagents operating within this repository.

---

## 1. Operating Rules

1. **Inspect Before Mutating**: Always inspect whole target files using `view_file` before attempting modifications.
2. **Preserve Compatibility**: Ensure 100% backward compatibility for all API parameters, NVS preference keys, and MQTT payload JSON fields.
3. **Verify Clean Syntax**: Double check that all header includes, namespaces, and variable types are valid C++17 for ESP32.
4. **No AI Text Over-Commenting**: Maintain concise, professional open-source comment formatting explaining *why*, not *what*.

---

## 2. File Modification Boundaries

- **Rarely Modify**: `Config.h` (pin mapping and default timing constants), `SystemManager.cpp` (state evaluation thresholds).
- **Safe To Extend**: `SensorManager` (adding new sensors), `DisplayManager` (adding new UI pages), `MQTTManager` (adding telemetry fields).
