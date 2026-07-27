#pragma once

#include "Config.h"

class SystemManager {
public:
    SystemManager();
    void begin();
    
    // Core state evaluation
    void evaluate(
        uint16_t mq2, uint16_t mq4, uint16_t mq135,
        float temp, float hum,
        bool isHeating, bool hasFault,
        const Config::DeviceConfig& config
    );

    Config::SystemState getCurrentState() const { return m_currentState; }
    Config::SystemState getPreviousState() const { return m_previousState; }

    bool hasStateChanged() const { return m_currentState != m_previousState; }

private:
    // Hysteresis helper
    bool checkThreshold(uint16_t value, int thresholdVal, bool currentLatching) const;
    
    // Rate of Rise evaluation
    void calculateRateOfRise(uint16_t mq2, uint16_t mq4, uint16_t mq135);

    Config::SystemState m_currentState;
    Config::SystemState m_previousState;

    // Latches for hysteresis
    bool m_mq2WarningLatch;
    bool m_mq2DangerLatch;
    bool m_mq4WarningLatch;
    bool m_mq4DangerLatch;
    bool m_mq135WarningLatch;
    bool m_mq135DangerLatch;

    // Rate of Rise Variables
    uint32_t m_lastRoRCalc;
    uint16_t m_prevMq2;
    uint16_t m_prevMq4;
    uint16_t m_prevMq135;
    float m_mq2RoR;
    float m_mq4RoR;
    float m_mq135RoR;
    bool m_rorAlert;
};
