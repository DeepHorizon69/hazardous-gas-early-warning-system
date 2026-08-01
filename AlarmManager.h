#pragma once

#include "Config.h"

class AlarmManager {
public:
    AlarmManager();
    void begin();

    void update(Config::SystemState state, const Config::DeviceConfig& deviceConfig);
    void silence();

private:
    void buzzerOn();
    void buzzerOff();
    void updatePulsedBuzzer(uint32_t onTimeMs, uint32_t offTimeMs);
    void updateWarningBuzzer();
    void updateFaultBuzzer();

    bool m_buzzerState;
    uint32_t m_lastToggle;
    bool m_silenced;
};
