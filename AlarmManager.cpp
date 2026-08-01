#include "AlarmManager.h"
#include "Logger.h"

static const char* TAG = "AlarmManager";

AlarmManager::AlarmManager()
    : m_buzzerState(false), m_lastToggle(0), m_silenced(false) {}

void AlarmManager::begin() {
    pinMode(Config::PIN_BUZZER, OUTPUT);
    buzzerOff();
    LOGI(TAG, "AlarmManager initialized");
}

void AlarmManager::buzzerOn() {
    if (!m_buzzerState) {
        digitalWrite(Config::PIN_BUZZER, HIGH);
        m_buzzerState = true;
    }
}

void AlarmManager::buzzerOff() {
    if (m_buzzerState) {
        digitalWrite(Config::PIN_BUZZER, LOW);
        m_buzzerState = false;
    }
}

void AlarmManager::silence() {
    m_silenced = true;
    buzzerOff();
    LOGW(TAG, "Alarm buzzer manually silenced");
}

void AlarmManager::updatePulsedBuzzer(uint32_t onTimeMs, uint32_t offTimeMs) {
    uint32_t now = millis();
    uint32_t interval = m_buzzerState ? onTimeMs : offTimeMs;
    if (now - m_lastToggle >= interval) {
        m_lastToggle = now;
        if (m_buzzerState) {
            buzzerOff();
        } else {
            buzzerOn();
        }
    }
}

void AlarmManager::updateWarningBuzzer() {
    updatePulsedBuzzer(200, 800); // 200ms ON, 800ms OFF
}

void AlarmManager::updateFaultBuzzer() {
    updatePulsedBuzzer(100, 2500); // 100ms ON, 2500ms OFF
}

void AlarmManager::update(Config::SystemState state, const Config::DeviceConfig& deviceConfig) {
    if (!deviceConfig.buzzerEnabled || m_silenced) {
        buzzerOff();
        return;
    }

    switch (state) {
        case Config::SystemState::BOOT:
        case Config::SystemState::HEATING:
        case Config::SystemState::SAFE:
        case Config::SystemState::NORMAL:
            buzzerOff();
            m_silenced = false;
            break;

        case Config::SystemState::MODERATE:
            updateFaultBuzzer();
            break;

        case Config::SystemState::WARNING:
            updateWarningBuzzer();
            break;

        case Config::SystemState::DANGER:
        case Config::SystemState::CRITICAL:
            buzzerOn();
            break;

        case Config::SystemState::FAULT:
            updateFaultBuzzer();
            break;

        default:
            buzzerOff();
            break;
    }
}
