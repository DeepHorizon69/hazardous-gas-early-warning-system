#include "Config.h"
#include "Logger.h"
#include "StorageManager.h"
#include "SensorManager.h"
#include "AlarmManager.h"
#include "DisplayManager.h"
#include "NetworkManager.h"
#include "MQTTManager.h"
#include "SystemManager.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

static const char* TAG = "MainEntry";

namespace Config {
    const char* WIFI_SSID     = "DAVINCI";
    const char* WIFI_PASSWORD = "DUKEHORNP";
    const char* MQTT_SERVER   = "broker.emqx.io";
    uint16_t MQTT_PORT        = 1883;
    const char* MQTT_NAMESPACE= "alsa";
}

static StorageManager storage;
static SensorManager sensors;
static AlarmManager alarms;
static DisplayManager display;
static IoTNetworkManager network;
static MQTTManager mqtt;
static SystemManager sys;

static Config::DeviceConfig activeConfig;
static SemaphoreHandle_t configMutex = nullptr;

static volatile Config::SystemState sharedSystemState = Config::SystemState::BOOT;
static volatile bool wifiStatus = false;
static volatile bool mqttStatus = false;
static volatile bool hasStatePendingPublish = false;

static TaskHandle_t sensorTaskHandle = nullptr;
static TaskHandle_t networkTaskHandle = nullptr;

void runSensorSafetyTask(void* parameter);
void runCommunicationTask(void* parameter);

void onConfigChangeReceived(const Config::DeviceConfig& newCfg) {
    if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        activeConfig = newCfg;
        storage.saveConfig(activeConfig);
        display.invalidate();
        xSemaphoreGive(configMutex);

        LOGI(TAG, "New config applied and saved to NVS successfully");
        mqtt.publishConfigAck(activeConfig);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    LOGI(TAG, "===============================================");
    LOGI(TAG, "  EARLY GAS DETECTION SYSTEM - BOOT INITIALIZED ");
    LOGI(TAG, "===============================================");

    Logger::init();
    configMutex = xSemaphoreCreateMutex();

    storage.begin();
    storage.loadConfig(activeConfig);

    alarms.begin();
    display.begin();
    sensors.begin();
    sys.begin();

    network.begin();
    mqtt.begin(onConfigChangeReceived);

    xTaskCreatePinnedToCore(
        runSensorSafetyTask,
        "SensorSafetyTask",
        4096,
        NULL,
        5,
        &sensorTaskHandle,
        1
    );

    xTaskCreatePinnedToCore(
        runCommunicationTask,
        "NetworkCommTask",
        4096,
        NULL,
        1,
        &networkTaskHandle,
        0
    );

    LOGI(TAG, "Multitasking Execution Tasks Scheduled Successfully");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void runSensorSafetyTask(void* parameter) {
    (void)parameter;
    LOGI(TAG, "Sensor & Safety Task started on Core %d", xPortGetCoreID());

    TickType_t lastWakeTime = xTaskGetTickCount();
    Config::DeviceConfig localConfig;

    while (true) {
        if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            localConfig = activeConfig;
            xSemaphoreGive(configMutex);
        }

        sensors.update(localConfig);

        sys.evaluate(
            sensors.getMQ2Filtered(),
            sensors.getMQ4Filtered(),
            sensors.getMQ135Filtered(),
            sensors.getTemperature(),
            sensors.getHumidity(),
            sensors.isHeating(),
            sensors.hasAnyFault(),
            localConfig
        );

        sharedSystemState = sys.getCurrentState();

        if (sys.hasStateChanged()) {
            hasStatePendingPublish = true;
        }

        alarms.update(sharedSystemState, localConfig);

        display.update(
            sharedSystemState,
            sensors.getMQ2Filtered(),
            sensors.getMQ4Filtered(),
            sensors.getMQ135Filtered(),
            sensors.getTemperature(),
            sensors.getHumidity(),
            sensors.getHeatingProgress(),
            sensors.getHeatingRemaining(),
            wifiStatus,
            mqttStatus,
            sensors.hasAnyFault()
        );

        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(Config::SENSOR_READ_INTERVAL_MS));
    }
}

void runCommunicationTask(void* parameter) {
    (void)parameter;
    LOGI(TAG, "Network & Comm Task started on Core %d", xPortGetCoreID());

    uint32_t lastTelemetryPub = 0;
    Config::DeviceConfig localConfig;

    while (true) {
        network.update();
        wifiStatus = network.isConnected();

        mqtt.update(wifiStatus);
        mqttStatus = mqtt.isConnected();

        if (mqttStatus && hasStatePendingPublish) {
            hasStatePendingPublish = false;
            mqtt.publishState(sharedSystemState);
        }

        uint32_t now = millis();
        if (wifiStatus && mqttStatus && (now - lastTelemetryPub >= Config::TELEMETRY_INTERVAL_MS)) {
            lastTelemetryPub = now;

            if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                localConfig = activeConfig;
                xSemaphoreGive(configMutex);
            }

            mqtt.publishTelemetry(
                sensors.getMQ2Filtered(),
                sensors.getMQ4Filtered(),
                sensors.getMQ135Filtered(),
                sensors.getTemperature(),
                sensors.getHumidity(),
                sharedSystemState,
                localConfig,
                sensors.hasMQ2Fault(),
                sensors.hasMQ4Fault(),
                sensors.hasMQ135Fault(),
                sensors.hasDHTFault(),
                sensors.getHeatingProgress(),
                sensors.getHeatingRemaining()
            );
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
