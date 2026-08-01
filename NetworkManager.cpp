#include "NetworkManager.h"
#include <WiFi.h>
#include "Logger.h"

static const char* TAG = "NetworkManager";

IoTNetworkManager::IoTNetworkManager() :
    m_lastWiFiReconnect(0),
    m_reconnectDelay(Config::WIFI_RECONNECT_MIN_MS),
    m_connecting(true)
{}

void IoTNetworkManager::begin() {
    LOGI(TAG, "Initializing WiFi in Station Mode...");
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
    m_lastWiFiReconnect = millis();
}

bool IoTNetworkManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

void IoTNetworkManager::handleDisconnectedState() {
    uint32_t now = millis();

    if (now - m_lastWiFiReconnect < m_reconnectDelay) {
        return;
    }
    m_lastWiFiReconnect = now;

    m_reconnectDelay *= 2;
    if (m_reconnectDelay > Config::WIFI_RECONNECT_MAX_MS) {
        m_reconnectDelay = Config::WIFI_RECONNECT_MAX_MS;
    }

    LOGW(TAG, "WiFi disconnected. Retrying in %lu ms...", m_reconnectDelay);

    WiFi.disconnect(false, true);
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
}

void IoTNetworkManager::update() {
    if (isConnected()) {
        if (m_connecting) {
            m_connecting = false;
            m_reconnectDelay = Config::WIFI_RECONNECT_MIN_MS;
            LOGI(TAG, "WiFi Connected. IP Address: %s", WiFi.localIP().toString().c_str());
        }
    } else {
        m_connecting = true;
        handleDisconnectedState();
    }
}
