#include "SystemManager.h"
#include "Logger.h"

static const char* TAG = "SystemManager";

SystemManager::SystemManager() :
    m_currentState(Config::SystemState::BOOT),
    m_previousState(Config::SystemState::BOOT),
    m_mq2WarningLatch(false), m_mq2DangerLatch(false),
    m_mq4WarningLatch(false), m_mq4DangerLatch(false),
    m_mq135WarningLatch(false), m_mq135DangerLatch(false),
    m_lastRoRCalc(0),
    m_prevMq2(0), m_prevMq4(0), m_prevMq135(0),
    m_mq2RoR(0.0f), m_mq4RoR(0.0f), m_mq135RoR(0.0f),
    m_rorAlert(false)
{}

void SystemManager::begin() {
    m_currentState = Config::SystemState::BOOT;
    m_previousState = Config::SystemState::BOOT;
    m_lastRoRCalc = millis();
    LOGI(TAG, "SystemManager initialized");
}

bool SystemManager::checkThreshold(uint16_t value, int thresholdVal, bool currentLatching) const {
    if (currentLatching) {
        return value >= (thresholdVal - Config::HYSTERESIS_VAL);
    } else {
        return value >= thresholdVal;
    }
}

void SystemManager::calculateRateOfRise(uint16_t mq2, uint16_t mq4, uint16_t mq135) {
    uint32_t now = millis();
    uint32_t dt_ms = now - m_lastRoRCalc;

    if (dt_ms < 1000UL) return;

    m_lastRoRCalc = now;
    float dt = dt_ms / 1000.0f;

    m_mq2RoR = static_cast<float>(mq2 - m_prevMq2) / dt;
    m_mq4RoR = static_cast<float>(mq4 - m_prevMq4) / dt;
    m_mq135RoR = static_cast<float>(mq135 - m_prevMq135) / dt;

    m_prevMq2 = mq2;
    m_prevMq4 = mq4;
    m_prevMq135 = mq135;

    m_rorAlert = (m_mq2RoR >= Config::RATE_OF_RISE_THRESHOLD ||
                  m_mq4RoR >= Config::RATE_OF_RISE_THRESHOLD ||
                  m_mq135RoR >= Config::RATE_OF_RISE_THRESHOLD);

    if (m_rorAlert) {
        LOGW(TAG, "Sudden gas increase detected! RoR: MQ2=%.1f MQ4=%.1f MQ135=%.1f", m_mq2RoR, m_mq4RoR, m_mq135RoR);
    }
}

void SystemManager::evaluate(
    uint16_t mq2, uint16_t mq4, uint16_t mq135,
    float temp, float hum,
    bool isHeating, bool hasFault,
    const Config::DeviceConfig& config
) {
    m_previousState = m_currentState;

    if (hasFault) {
        m_currentState = Config::SystemState::FAULT;
        return;
    }

    if (isHeating) {
        m_currentState = Config::SystemState::HEATING;
        return;
    }

    calculateRateOfRise(mq2, mq4, mq135);

    if (config.mq2Enabled) {
        m_mq2WarningLatch = checkThreshold(mq2, config.mq2Off, m_mq2WarningLatch);
        m_mq2DangerLatch = checkThreshold(mq2, config.mq2On, m_mq2DangerLatch);
    } else {
        m_mq2WarningLatch = false;
        m_mq2DangerLatch = false;
    }

    if (config.mq4Enabled) {
        m_mq4WarningLatch = checkThreshold(mq4, config.mq4Off, m_mq4WarningLatch);
        m_mq4DangerLatch = checkThreshold(mq4, config.mq4On, m_mq4DangerLatch);
    } else {
        m_mq4WarningLatch = false;
        m_mq4DangerLatch = false;
    }

    if (config.mq135Enabled) {
        m_mq135WarningLatch = checkThreshold(mq135, config.mq135Off, m_mq135WarningLatch);
        m_mq135DangerLatch = checkThreshold(mq135, config.mq135On, m_mq135DangerLatch);
    } else {
        m_mq135WarningLatch = false;
        m_mq135DangerLatch = false;
    }

    uint8_t dangerCount = (m_mq2DangerLatch ? 1 : 0) + (m_mq4DangerLatch ? 1 : 0) + (m_mq135DangerLatch ? 1 : 0);
    bool extremeTemp = (config.dhtEnabled && !isnan(temp) && temp >= 50.0f);

    if (dangerCount >= 2 || extremeTemp) {
        m_currentState = Config::SystemState::CRITICAL;
        return;
    }

    if (m_mq2DangerLatch || m_mq4DangerLatch || m_mq135DangerLatch) {
        m_currentState = Config::SystemState::DANGER;
        return;
    }

    bool tempWarning = (config.dhtEnabled && !isnan(temp) && temp >= config.tempWarn);
    if (m_mq2WarningLatch || m_mq4WarningLatch || m_mq135WarningLatch || tempWarning) {
        m_currentState = Config::SystemState::WARNING;
        return;
    }

    if (m_rorAlert) {
        m_currentState = Config::SystemState::MODERATE;
        return;
    }

    bool isMQ2Safe = !config.mq2Enabled || (mq2 < (config.mq2Off - Config::SAFE_MARGIN));
    bool isMQ4Safe = !config.mq4Enabled || (mq4 < (config.mq4Off - Config::SAFE_MARGIN));
    bool isMQ135Safe = !config.mq135Enabled || (mq135 < (config.mq135Off - Config::SAFE_MARGIN));

    if (isMQ2Safe && isMQ4Safe && isMQ135Safe) {
        m_currentState = Config::SystemState::SAFE;
    } else {
        m_currentState = Config::SystemState::NORMAL;
    }
}
