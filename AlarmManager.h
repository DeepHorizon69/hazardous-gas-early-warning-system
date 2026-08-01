#pragma once

#include "Config.h"

class AlarmManager {
public:
    AlarmManager();
    void begin();
    
    // Updates buzzer based on system state
    void update(Config::SystemState state, const Config::DeviceConfig& deviceConfig);
    
    // Silence alarms manually
    void silence();

private:
    void buzzerOn();
    void buzzerOff();
    void updateWarningBuzzer();
    void updateFaultBuzzer();

    bool m_buzzerState;
    uint32_t m_lastToggle;
    bool m_silenced;
};
