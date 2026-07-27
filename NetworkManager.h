#pragma once

#include "Config.h"

class IoTNetworkManager {
public:
    IoTNetworkManager();
    void begin();
    
    // Maintain connection in a non-blocking way
    void update();
    
    bool isConnected() const;

private:
    void handleDisconnectedState();

    uint32_t m_lastWiFiReconnect;
    uint32_t m_reconnectDelay;
    bool m_connecting;
};
