# Mandatory Project Rules & Standards

All modifications to this repository must adhere to the following strict rules:

---

## 1. Architecture Constraints

- **No Business Logic in Drivers**: Peripherals (OLED, DHT, WiFi) must be accessed via Manager classes.
- **No Direct GPIO Access in Safety Logic**: `SystemManager` must only compute states from numeric inputs. Pin toggles belong in `AlarmManager` or hardware drivers.
- **Network Independence**: The system must operate fully stand-alone. Loss of WiFi or MQTT must **never** block or delay Core 1 sensor execution.

---

## 2. Embedded & Memory Guidelines

- **No Exceptions (`-fno-exceptions`)**: Use return codes or status booleans.
- **No Heap Allocation on Loop Paths**: Do not use `new`, `malloc`, or dynamic `String` concat inside `update()` or loop methods.
- **No `std::function`**: Use standard C-style function pointers.
- **Use `constexpr` & `const` references**: Keep constants in flash memory.

---

## 3. Safety Invariants

- **Do Not Modify Threshold Logic**: Hysteresis (`HYSTERESIS_VAL = 100`), Rate-of-Rise calculation (`50.0f`), and pre-heating duration (`300000ms`) must remain untouched unless authorized.
- **Buzzer Priority**: High-danger states (`CRITICAL`, `DANGER`) must override silencing locks when a new higher-tier state occurs.

---

## 4. Verification Requirements

Every pull request or commit must pass:
1. Clean build without compiler warnings.
2. NVS key compatibility check.
3. MQTT payload JSON structure verification.
