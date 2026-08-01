#include "MQTTManager.h"
#include <WiFi.h>
#include "Logger.h"

static const char* TAG = "MQTTManager";

MQTTManager::MQTTManager() :
    m_mqttClient(m_wifiClient),
    m_lastMQTTReconnect(0),
    m_reconnectDelay(Config::MQTT_RECONNECT_MIN_MS),
    m_subscribed(false),
    m_configCallback(nullptr)
{
    m_topicData[0] = '\0';
    m_topicState[0] = '\0';
    m_topicConfig[0] = '\0';
    m_topicConfigAck[0] = '\0';
}

void MQTTManager::begin(ConfigCallback callback) {
    m_configCallback = callback;
    buildTopics();

    m_mqttClient.setServer(Config::MQTT_SERVER, Config::MQTT_PORT);

    m_mqttClient.setCallback([this](char* topic, byte* payload, unsigned int length) {
        if (strcmp(topic, m_topicConfig) == 0) {
            this->processConfigPayload(payload, length);
        }
    });

    m_mqttClient.setBufferSize(1024);
    LOGI(TAG, "MQTT Manager ready");
}

void MQTTManager::buildTopics() {
    snprintf(m_topicData, sizeof(m_topicData), "%s/gas/data", Config::MQTT_NAMESPACE);
    snprintf(m_topicState, sizeof(m_topicState), "%s/gas/state", Config::MQTT_NAMESPACE);
    snprintf(m_topicConfig, sizeof(m_topicConfig), "%s/gas/config", Config::MQTT_NAMESPACE);
    snprintf(m_topicConfigAck, sizeof(m_topicConfigAck), "%s/gas/config/ack", Config::MQTT_NAMESPACE);
}

bool MQTTManager::isConnected() {
    return m_mqttClient.connected();
}

bool MQTTManager::connectMQTT() {
    if (m_mqttClient.connected()) {
        return true;
    }

    uint64_t mac = ESP.getEfuseMac();
    char clientId[32];
    snprintf(clientId, sizeof(clientId), "ESP32-GAS-%04X", (uint16_t)(mac & 0xFFFF));

    LOGI(TAG, "Attempting MQTT connection as %s...", clientId);

    bool ok = m_mqttClient.connect(clientId, m_topicState, 1, true, "FAULT");

    if (!ok) {
        LOGE(TAG, "MQTT Connection failed, rc=%d", m_mqttClient.state());
        return false;
    }

    LOGI(TAG, "MQTT Connected. Subscribing to config topic...");
    m_mqttClient.subscribe(m_topicConfig, 1);
    m_subscribed = true;

    return true;
}

void MQTTManager::update(bool wifiConnected) {
    if (!wifiConnected) {
        m_subscribed = false;
        return;
    }

    if (m_mqttClient.connected()) {
        m_mqttClient.loop();
        return;
    }

    uint32_t now = millis();
    if (now - m_lastMQTTReconnect >= m_reconnectDelay) {
        m_lastMQTTReconnect = now;

        if (connectMQTT()) {
            m_reconnectDelay = Config::MQTT_RECONNECT_MIN_MS;
        } else {
            m_reconnectDelay *= 2;
            if (m_reconnectDelay > Config::MQTT_RECONNECT_MAX_MS) {
                m_reconnectDelay = Config::MQTT_RECONNECT_MAX_MS;
            }
        }
    }
}

void MQTTManager::publishState(Config::SystemState state) {
    if (!m_mqttClient.connected()) return;

    m_mqttClient.publish(m_topicState, Config::getStateText(state), true);
    LOGI(TAG, "Published system state: %s", Config::getStateText(state));
}

void MQTTManager::publishTelemetry(
    uint16_t mq2, uint16_t mq4, uint16_t mq135,
    float temp, float hum,
    Config::SystemState state,
    const Config::DeviceConfig& currentCfg,
    bool mq2Fault, bool mq4Fault, bool mq135Fault, bool dhtFault,
    uint8_t heatProgress, uint32_t heatRemaining
) {
    if (!m_mqttClient.connected()) return;

    StaticJsonDocument<1024> doc;

    if (mq2Fault) doc["mq2"] = nullptr;
    else doc["mq2"] = mq2;

    if (mq4Fault) doc["mq4"] = nullptr;
    else doc["mq4"] = mq4;

    if (mq135Fault) doc["mq135"] = nullptr;
    else doc["mq135"] = mq135;

    if (isnan(temp)) doc["temp"] = nullptr;
    else doc["temp"] = temp;

    if (isnan(hum)) doc["hum"] = nullptr;
    else doc["hum"] = hum;

    doc["state"] = Config::getStateText(state);
    doc["mq2Enabled"] = currentCfg.mq2Enabled;
    doc["mq4Enabled"] = currentCfg.mq4Enabled;
    doc["mq135Enabled"] = currentCfg.mq135Enabled;
    doc["dhtEnabled"] = currentCfg.dhtEnabled;

    doc["mq2Fault"] = mq2Fault;
    doc["mq4Fault"] = mq4Fault;
    doc["mq135Fault"] = mq135Fault;
    doc["dht22Fault"] = dhtFault;

    doc["mq2On"] = currentCfg.mq2On;
    doc["mq2Off"] = currentCfg.mq2Off;
    doc["mq4On"] = currentCfg.mq4On;
    doc["mq4Off"] = currentCfg.mq4Off;
    doc["mq135On"] = currentCfg.mq135On;
    doc["mq135Off"] = currentCfg.mq135Off;
    doc["tempWarn"] = currentCfg.tempWarn;

    doc["heatingProgress"] = heatProgress;
    doc["heatingRemaining"] = heatRemaining;
    doc["heatingDuration"] = Config::HEATING_DURATION_MS / 1000UL;

    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["heap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000UL;
    doc["version"] = "1.1.0";

    char payload[1024];
    size_t len = serializeJson(doc, payload, sizeof(payload));
    m_mqttClient.publish(m_topicData, payload, len);
}

void MQTTManager::publishConfigAck(const Config::DeviceConfig& config) {
    if (!m_mqttClient.connected()) return;

    StaticJsonDocument<512> doc;

    JsonObject mq2Obj = doc.createNestedObject("mq2");
    mq2Obj["enabled"] = config.mq2Enabled;
    mq2Obj["on"] = config.mq2On;
    mq2Obj["off"] = config.mq2Off;

    JsonObject mq4Obj = doc.createNestedObject("mq4");
    mq4Obj["enabled"] = config.mq4Enabled;
    mq4Obj["on"] = config.mq4On;
    mq4Obj["off"] = config.mq4Off;

    JsonObject mq135Obj = doc.createNestedObject("mq135");
    mq135Obj["enabled"] = config.mq135Enabled;
    mq135Obj["on"] = config.mq135On;
    mq135Obj["off"] = config.mq135Off;

    JsonObject dhtObj = doc.createNestedObject("dht22");
    dhtObj["enabled"] = config.dhtEnabled;
    dhtObj["tempWarn"] = config.tempWarn;

    doc["buzzerEnabled"] = config.buzzerEnabled;

    char payload[512];
    size_t len = serializeJson(doc, payload, sizeof(payload));
    m_mqttClient.publish(m_topicConfigAck, payload, len);
}

void MQTTManager::processConfigPayload(const byte* payload, unsigned int length) {
    if (length == 0 || m_configCallback == nullptr) return;

    StaticJsonDocument<1024> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        LOGE(TAG, "Failed to parse incoming Config JSON: %s", err.c_str());
        return;
    }

    if (!doc.containsKey("mq2") || !doc.containsKey("mq4") ||
        !doc.containsKey("mq135") || !doc.containsKey("dht22") ||
        !doc.containsKey("buzzerEnabled")) {
        LOGE(TAG, "Config JSON validation error: Missing keys");
        return;
    }

    Config::DeviceConfig newCfg;

    JsonObject mq2 = doc["mq2"];
    newCfg.mq2Enabled = mq2["enabled"];
    newCfg.mq2On = mq2["on"];
    newCfg.mq2Off = mq2["off"];

    JsonObject mq4 = doc["mq4"];
    newCfg.mq4Enabled = mq4["enabled"];
    newCfg.mq4On = mq4["on"];
    newCfg.mq4Off = mq4["off"];

    JsonObject mq135 = doc["mq135"];
    newCfg.mq135Enabled = mq135["enabled"];
    newCfg.mq135On = mq135["on"];
    newCfg.mq135Off = mq135["off"];

    JsonObject dht22 = doc["dht22"];
    newCfg.dhtEnabled = dht22["enabled"];
    newCfg.tempWarn = dht22["tempWarn"];

    newCfg.buzzerEnabled = doc["buzzerEnabled"];

    if (newCfg.mq2On < newCfg.mq2Off || newCfg.mq4On < newCfg.mq4Off ||
        newCfg.mq135On < newCfg.mq135Off || newCfg.tempWarn <= 0) {
        LOGE(TAG, "Config JSON validation error: Threshold limits crossed");
        return;
    }

    LOGI(TAG, "Valid configuration payload processed. Applying changes...");
    m_configCallback(newCfg);
}
