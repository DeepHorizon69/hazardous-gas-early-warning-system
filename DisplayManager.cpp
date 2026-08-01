#include "DisplayManager.h"
#include <Wire.h>
#include "Logger.h"

static const char* TAG = "DisplayManager";

DisplayManager::DisplayManager() :
    m_display(Config::SCREEN_WIDTH, Config::SCREEN_HEIGHT, &Wire, -1),
    m_currentState(Config::SystemState::BOOT),
    m_lastState(Config::SystemState::BOOT),
    m_lastDraw(0),
    m_pageTimer(0),
    m_currentPage(Page::MONITOR),
    m_initialized(false),
    m_lastI2CCheck(0)
{
    m_cache[0] = '\0';
    m_buffer[0] = '\0';
}

void DisplayManager::begin() {
    if constexpr (!Config::OLED_ENABLED) {
        return;
    }
    Wire.begin(Config::PIN_I2C_SDA, Config::PIN_I2C_SCL);
    Wire.setClock(400000); // Set to fast I2C mode
    
    m_initialized = m_display.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDRESS);
    if (!m_initialized) {
        LOGE(TAG, "SSD1306 OLED initialization failed!");
    } else {
        m_display.clearDisplay();
        m_display.setTextColor(SSD1306_WHITE);
        m_display.setTextSize(1);
        m_display.display();
        LOGI(TAG, "OLED Display ready");
    }
    m_pageTimer = millis();
}

void DisplayManager::invalidate() {
    if constexpr (!Config::OLED_ENABLED) {
        return;
    }
    m_cache[0] = '\0';
}

void DisplayManager::drawProgressBar(int x, int y, int w, int h, uint8_t progress) {
    m_display.drawRect(x, y, w, h, SSD1306_WHITE);
    int fill = ((w - 2) * progress) / 100;
    m_display.fillRect(x + 1, y + 1, fill, h - 2, SSD1306_WHITE);
}

void DisplayManager::drawBootScreen() {
    m_display.clearDisplay();
    m_display.setTextSize(2);
    m_display.setCursor(8, 10);
    m_display.println("Gas");
    m_display.setCursor(8, 35);
    m_display.println("Monitor");
    m_display.setTextSize(1);
    m_display.setCursor(18, 58);
    m_display.println("Booting...");
    m_display.display();
}

void DisplayManager::drawHeatingScreen(uint8_t progress, uint32_t remaining) {
    uint8_t min = remaining / 60;
    uint8_t sec = remaining % 60;

    m_display.clearDisplay();
    m_display.setTextSize(2);
    m_display.setCursor(10, 0);
    m_display.println("HEATING");
    
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02u:%02u", min, sec);
    m_display.setCursor(20, 25);
    m_display.println(timeStr);

    drawProgressBar(5, 55, 118, 8, progress);
    m_display.display();
}

void DisplayManager::drawMonitorScreen(uint16_t mq2, uint16_t mq4, uint16_t mq135, Config::SystemState state) {
    m_display.clearDisplay();
    m_display.setTextSize(1);
    
    m_display.setCursor(0, 0);
    m_display.printf("MQ2  : %u", mq2);
    m_display.setCursor(0, 16);
    m_display.printf("MQ4  : %u", mq4);
    m_display.setCursor(0, 32);
    m_display.printf("MQ135: %u", mq135);

    m_display.drawLine(0, 48, 127, 48, SSD1306_WHITE);
    
    m_display.setTextSize(2);
    m_display.setCursor(0, 52);
    m_display.println(Config::getStateText(state));
    m_display.display();
}

void DisplayManager::drawNetworkScreen(float temp, float hum, bool wifiOk, bool mqttOk) {
    m_display.clearDisplay();
    m_display.setTextSize(1);

    m_display.setCursor(0, 0);
    if (isnan(temp)) m_display.println("Temp : --");
    else m_display.printf("Temp : %.1f C", temp);

    m_display.setCursor(0, 16);
    if (isnan(hum)) m_display.println("Hum  : --");
    else m_display.printf("Hum  : %.1f %%", hum);

    m_display.setCursor(0, 36);
    m_display.printf("WiFi : %s", wifiOk ? "OK" : "LOST");
    
    m_display.setCursor(0, 52);
    m_display.printf("MQTT : %s", mqttOk ? "OK" : "LOST");
    m_display.display();
}

void DisplayManager::drawFaultScreen() {
    m_display.clearDisplay();
    m_display.setTextSize(2);
    m_display.setCursor(10, 0);
    m_display.println("FAULT");
    m_display.setCursor(0, 30);
    m_display.println("DIAGNOSTIC");
    m_display.setTextSize(1);
    m_display.setCursor(0, 54);
    m_display.println("Check sensor units");
    m_display.display();
}

void DisplayManager::rotatePage(bool hasFault, Config::SystemState state) {
    if (hasFault || state == Config::SystemState::HEATING || state == Config::SystemState::BOOT) {
        return;
    }

    uint32_t duration = (m_currentPage == Page::MONITOR) ? Config::MONITOR_PAGE_TIME_MS : Config::NETWORK_PAGE_TIME_MS;
    uint32_t now = millis();
    if (now - m_pageTimer >= duration) {
        m_pageTimer = now;
        m_currentPage = (m_currentPage == Page::MONITOR) ? Page::NETWORK : Page::MONITOR;
    }
}

bool DisplayManager::verifyI2CBus() {
    Wire.beginTransmission(Config::OLED_ADDRESS);
    return Wire.endTransmission() == 0;
}

void DisplayManager::recoverI2CBus() {
    LOGW(TAG, "I2C lockup detected. Forcing bus recovery...");
    Wire.end();
    delay(30);
    Wire.begin(Config::PIN_I2C_SDA, Config::PIN_I2C_SCL);
    Wire.setClock(400000);
    delay(30);
    m_initialized = m_display.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDRESS);
    if (m_initialized) {
        LOGI(TAG, "OLED recovered and re-initialized");
        invalidate();
    } else {
        LOGE(TAG, "OLED recovery failed!");
    }
}

void DisplayManager::update(
    Config::SystemState state, 
    uint16_t mq2, uint16_t mq4, uint16_t mq135,
    float temp, float hum,
    uint8_t heatProgress, uint32_t heatRemaining,
    bool wifiOk, bool mqttOk,
    bool hasFault
) {
    if constexpr (!Config::OLED_ENABLED) {
        return;
    }
    uint32_t now = millis();

    // Check display timing (500ms intervals)
    if (now - m_lastDraw < Config::OLED_INTERVAL_MS) {
        return;
    }
    m_lastDraw = now;

    // Periodically verify I2C health
    if (now - m_lastI2CCheck >= 5000UL) {
        m_lastI2CCheck = now;
        if (!verifyI2CBus()) {
            recoverI2CBus();
        }
    }

    if (!m_initialized) return;

    rotatePage(hasFault, state);

    // Build unique display buffer representation for change check
    if (state == Config::SystemState::BOOT) {
        snprintf(m_buffer, sizeof(m_buffer), "BOOT");
    } else if (state == Config::SystemState::HEATING) {
        snprintf(m_buffer, sizeof(m_buffer), "HEAT_%u_%u", heatProgress, heatRemaining);
    } else if (hasFault) {
        snprintf(m_buffer, sizeof(m_buffer), "FAULT");
    } else if (m_currentPage == Page::MONITOR) {
        snprintf(m_buffer, sizeof(m_buffer), "MON_%u_%u_%u_%u", mq2, mq4, mq135, (uint8_t)state);
    } else {
        snprintf(m_buffer, sizeof(m_buffer), "NET_%.1f_%.1f_%d_%d", temp, hum, wifiOk, mqttOk);
    }

    // Skip drawing if content matches cached values (flicker protection)
    if (strcmp(m_buffer, m_cache) == 0) {
        return;
    }
    
    // Cache next content representation
    strncpy(m_cache, m_buffer, sizeof(m_cache) - 1);
    m_cache[sizeof(m_cache) - 1] = '\0';

    // Render screen
    if (state == Config::SystemState::BOOT) {
        drawBootScreen();
    } else if (state == Config::SystemState::HEATING) {
        drawHeatingScreen(heatProgress, heatRemaining);
    } else if (hasFault) {
        drawFaultScreen();
    } else if (m_currentPage == Page::MONITOR) {
        drawMonitorScreen(mq2, mq4, mq135, state);
    } else {
        drawNetworkScreen(temp, hum, wifiOk, mqttOk);
    }
}
