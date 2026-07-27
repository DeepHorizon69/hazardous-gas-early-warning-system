#pragma once

#include "Config.h"
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

class MQTTManager {
public:
    typedef std::function<void(const Config::DeviceConfig&)> ConfigCallback;

    MQTTManager();
    void begin(ConfigCallback callback);
    
    // Updates MQTT state machine and parses incoming commands
    void update(bool wifiConnected);

    bool isConnected();

    // Publish methods
    void publishTelemetry(
        uint16_t mq2, uint16_t mq4, uint16_t mq135,
        float temp, float hum,
        Config::SystemState state,
        const Config::DeviceConfig& currentCfg,
        bool mq2Fault, bool mq4Fault, bool mq135Fault, bool dhtFault,
        uint8_t heatProgress, uint32_t heatRemaining
    );
    
    void publishState(Config::SystemState state);
    void publishConfigAck(const Config::DeviceConfig& config);

private:
    void buildTopics();
    bool connectMQTT();
    void processConfigPayload(const byte* payload, unsigned int length);

    WiFiClient m_wifiClient;
    PubSubClient m_mqttClient;

    // Topics buffers
    char m_topicData[64];
    char m_topicState[64];
    char m_topicConfig[64];
    char m_topicConfigAck[64];

    uint32_t m_lastMQTTReconnect;
    uint32_t m_reconnectDelay;
    bool m_subscribed;

    ConfigCallback m_configCallback;
};
