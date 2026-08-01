#pragma once

#include "Config.h"
#include <Adafruit_SSD1306.h>

class DisplayManager {
public:
    DisplayManager();
    void begin();

    void update(
        Config::SystemState state,
        uint16_t mq2, uint16_t mq4, uint16_t mq135,
        float temp, float hum,
        uint8_t heatProgress, uint32_t heatRemaining,
        bool wifiOk, bool mqttOk,
        bool hasFault
    );

    void invalidate();

private:
    enum class Page : uint8_t {
        MONITOR = 0,
        NETWORK
    };

    struct DisplaySnapshot {
        Config::SystemState state;
        Page page;
        uint16_t mq2, mq4, mq135;
        float temp, hum;
        uint8_t heatProgress;
        uint32_t heatRemaining;
        bool wifiOk, mqttOk;
        bool hasFault;

        bool equals(const DisplaySnapshot& o) const {
            return state == o.state &&
                   page == o.page &&
                   mq2 == o.mq2 && mq4 == o.mq4 && mq135 == o.mq135 &&
                   temp == o.temp && hum == o.hum &&
                   heatProgress == o.heatProgress && heatRemaining == o.heatRemaining &&
                   wifiOk == o.wifiOk && mqttOk == o.mqttOk &&
                   hasFault == o.hasFault;
        }
    };

    void drawBootScreen();
    void drawHeatingScreen(uint8_t progress, uint32_t remaining);
    void drawMonitorScreen(uint16_t mq2, uint16_t mq4, uint16_t mq135, Config::SystemState state);
    void drawNetworkScreen(float temp, float hum, bool wifiOk, bool mqttOk);
    void drawFaultScreen();

    void drawProgressBar(int x, int y, int w, int h, uint8_t progress);
    void rotatePage(bool hasFault, Config::SystemState state);

    bool verifyI2CBus();
    void recoverI2CBus();

    Adafruit_SSD1306 m_display;
    uint32_t m_lastDraw;
    uint32_t m_pageTimer;
    Page m_currentPage;

    DisplaySnapshot m_lastSnapshot;
    bool m_dirty;
    bool m_initialized;
    uint32_t m_lastI2CCheck;
};
