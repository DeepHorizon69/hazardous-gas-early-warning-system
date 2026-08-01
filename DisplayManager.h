#pragma once

#include "Config.h"
#include <Adafruit_SSD1306.h>

class DisplayManager {
public:
    DisplayManager();
    void begin();
    
    // Updates display layout based on state
    void update(
        Config::SystemState state, 
        uint16_t mq2, uint16_t mq4, uint16_t mq135,
        float temp, float hum,
        uint8_t heatProgress, uint32_t heatRemaining,
        bool wifiOk, bool mqttOk,
        bool hasFault
    );

    // Forces screen redraw next refresh
    void invalidate();

private:
    void drawBootScreen();
    void drawHeatingScreen(uint8_t progress, uint32_t remaining);
    void drawMonitorScreen(uint16_t mq2, uint16_t mq4, uint16_t mq135, Config::SystemState state);
    void drawNetworkScreen(float temp, float hum, bool wifiOk, bool mqttOk);
    void drawFaultScreen();
    
    void drawProgressBar(int x, int y, int w, int h, uint8_t progress);
    
    // Non-blocking page switching
    void rotatePage(bool hasFault, Config::SystemState state);
    
    // I2C health checks and reset logic
    bool verifyI2CBus();
    void recoverI2CBus();

    Adafruit_SSD1306 m_display;
    Config::SystemState m_currentState;
    Config::SystemState m_lastState;
    uint32_t m_lastDraw;
    uint32_t m_pageTimer;
    enum class Page : uint8_t {
        MONITOR = 0,
        NETWORK
    } m_currentPage;

    char m_cache[128];
    char m_buffer[128];
    bool m_initialized;
    uint32_t m_lastI2CCheck;
};
