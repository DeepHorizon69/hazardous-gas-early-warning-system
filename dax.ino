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

// Global Namespace Constants definitions
namespace Config {
    const char* WIFI_SSID = "DAVINCI";
    const char* WIFI_PASSWORD = "DUKEHORNP";
    const char* MQTT_SERVER = "broker.emqx.io";
    uint16_t MQTT_PORT = 1883;
    const char* MQTT_NAMESPACE = "alsa";
}

// Global Manager instances
static StorageManager storage;
static SensorManager sensors;
static AlarmManager alarms;
static DisplayManager display;
static IoTNetworkManager network;
static MQTTManager mqtt;
static SystemManager sys;

// Thread-safe variables
static Config::DeviceConfig activeConfig;
static SemaphoreHandle_t configMutex = nullptr;

// Thread shared status variables
static volatile Config::SystemState sharedSystemState = Config::SystemState::BOOT;
static volatile bool wifiStatus = false;
static volatile bool mqttStatus = false;
static volatile bool hasStatePendingPublish = false;

// FreeRTOS Task Handles
static TaskHandle_t sensorTaskHandle = nullptr;
static TaskHandle_t networkTaskHandle = nullptr;

// Forward declaration of tasks
void runSensorSafetyTask(void* parameter);
void runCommunicationTask(void* parameter);

// Configuration callback triggered by MQTT Manager
void onConfigChangeReceived(const Config::DeviceConfig& newCfg) {
    if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        activeConfig = newCfg;
        storage.saveConfig(activeConfig);
        display.invalidate(); // Force screen clear to update new configurations
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

    // Initialize mutexes and thread locks
    Logger::init();
    configMutex = xSemaphoreCreateMutex();

    // Start local peripherals
    storage.begin();
    storage.loadConfig(activeConfig);

    alarms.begin();
    display.begin();
    sensors.begin();
    sys.begin();

    // Initialize Network and MQTT Client
    network.begin();
    mqtt.begin(onConfigChangeReceived);

    // Create Safety-Critical Sensor Task on Core 1 (High Priority: 5)
    xTaskCreatePinnedToCore(
        runSensorSafetyTask,
        "SensorSafetyTask",
        4096,
        NULL,
        5,
        &sensorTaskHandle,
        1
    );

    // Create Communications Task on Core 0 (Low Priority: 1)
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
    // Arduino loop yields itself immediately. Everything runs inside FreeRTOS Tasks.
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// -------------------------------------------------------------
// CORE 1 TASK: SENSORS & SAFETY-CRITICAL ALARMS
// -------------------------------------------------------------
void runSensorSafetyTask(void* parameter) {
    LOGI(TAG, "Sensor & Safety Task started on Core %d", xPortGetCoreID());
    
    TickType_t lastWakeTime = xTaskGetTickCount();
    Config::DeviceConfig localConfig;

    while (true) {
        // Copy config in a thread-safe block
        if (xSemaphoreTake(configMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            localConfig = activeConfig;
            xSemaphoreGive(configMutex);
        }

        // 1. Process sensor acquisition and filters (EMA + Median)
        sensors.update(localConfig);

        // 2. Evaluate hazard thresholds, hysteresis, and rate-of-rise
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

        // Trigger network status transition publish immediately on state changes
        if (sys.hasStateChanged()) {
            hasStatePendingPublish = true;
        }

        // 3. Drive safety buzzer outputs
        alarms.update(sharedSystemState, localConfig);

        // 4. Drive local OLED screen layouts
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

        // Yield CPU thread for exactly 100ms (10Hz sampling frequency)
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(Config::SENSOR_READ_INTERVAL_MS));
    }
}

// -------------------------------------------------------------
// CORE 0 TASK: WIFI, MQTT, & TELEMETRY COMMUNICATIONS
// -------------------------------------------------------------
void runCommunicationTask(void* parameter) {
    LOGI(TAG, "Network & Comm Task started on Core %d", xPortGetCoreID());
    
    uint32_t lastTelemetryPub = 0;
    Config::DeviceConfig localConfig;

    while (true) {
        // 1. Run Non-blocking WiFi reconnect state machine
        network.update();
        wifiStatus = network.isConnected();

        // 2. Run Non-blocking MQTT loop and reconnect broker state machine
        mqtt.update(wifiStatus);
        mqttStatus = mqtt.isConnected();

        // 3. Publish safety state shifts immediately (event-driven)
        if (mqttStatus && hasStatePendingPublish) {
            hasStatePendingPublish = false;
            mqtt.publishState(sharedSystemState);
        }

        // 4. Publish Telemetry Stream (Periodic - default 2 seconds)
        uint32_t now = millis();
        if (wifiStatus && mqttStatus && (now - lastTelemetryPub >= Config::TELEMETRY_INTERVAL_MS)) {
            lastTelemetryPub = now;

            // Fetch device config safely
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

        // Yield network tasks thread briefly to prevent starvation (50ms sleep)
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
