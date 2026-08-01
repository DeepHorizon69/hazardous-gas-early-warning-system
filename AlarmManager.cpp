#include "AlarmManager.h"
#include "Logger.h"

static const char* TAG = "AlarmManager";

AlarmManager::AlarmManager() : m_buzzerState(false), m_lastToggle(0), m_silenced(false) {}

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

void AlarmManager::updateWarningBuzzer() {
    uint32_t now = millis();
    if (!m_buzzerState) {
        if (now - m_lastToggle >= 800) { // Off for 800ms
            buzzerOn();
            m_lastToggle = now;
        }
    } else {
        if (now - m_lastToggle >= 200) { // On for 200ms
            buzzerOff();
            m_lastToggle = now;
        }
    }
}

void AlarmManager::updateFaultBuzzer() {
    uint32_t now = millis();
    if (!m_buzzerState) {
        if (now - m_lastToggle >= 2500) { // Off for 2.5 seconds
            buzzerOn();
            m_lastToggle = now;
        }
    } else {
        if (now - m_lastToggle >= 100) { // On for 100ms (short blip)
            buzzerOff();
            m_lastToggle = now;
        }
    }
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
            m_silenced = false; // Reset silencing lock when state is safe
            break;

        case Config::SystemState::MODERATE:
            // Slow warning blip
            updateFaultBuzzer(); 
            break;

        case Config::SystemState::WARNING:
            updateWarningBuzzer(); // Pulsed warning
            break;

        case Config::SystemState::DANGER:
        case Config::SystemState::CRITICAL:
            buzzerOn(); // Solid Danger/Critical alert
            break;

        case Config::SystemState::FAULT:
            updateFaultBuzzer(); // Intermittent Fault Blip
            break;

        default:
            buzzerOff();
            break;
    }
}
