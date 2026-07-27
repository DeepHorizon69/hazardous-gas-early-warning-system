#include "StorageManager.h"
#include <Preferences.h>
#include "Logger.h"

static const char* TAG = "StorageManager";
static Preferences prefs;

StorageManager::StorageManager() {}

void StorageManager::begin() {
    // Preferences automatically handles init
}

void StorageManager::loadDefaultConfig(Config::DeviceConfig& deviceConfig) {
    deviceConfig.mq2Enabled = true;
    deviceConfig.mq2On = 2500;
    deviceConfig.mq2Off = 1800;

    deviceConfig.mq4Enabled = true;
    deviceConfig.mq4On = 2500;
    deviceConfig.mq4Off = 1800;

    deviceConfig.mq135Enabled = true;
    deviceConfig.mq135On = 2500;
    deviceConfig.mq135Off = 1800;

    deviceConfig.dhtEnabled = true;
    deviceConfig.tempWarn = 40.0f;

    deviceConfig.buzzerEnabled = true;
    LOGI(TAG, "Default configuration loaded");
}

void StorageManager::loadConfig(Config::DeviceConfig& deviceConfig) {
    loadDefaultConfig(deviceConfig); // Fallback to defaults first

    if (prefs.begin(Config::NVS_NAMESPACE, true)) {
        deviceConfig.mq2Enabled = prefs.getBool("mq2_en", deviceConfig.mq2Enabled);
        deviceConfig.mq2On = prefs.getInt("mq2_on", deviceConfig.mq2On);
        deviceConfig.mq2Off = prefs.getInt("mq2_off", deviceConfig.mq2Off);

        deviceConfig.mq4Enabled = prefs.getBool("mq4_en", deviceConfig.mq4Enabled);
        deviceConfig.mq4On = prefs.getInt("mq4_on", deviceConfig.mq4On);
        deviceConfig.mq4Off = prefs.getInt("mq4_off", deviceConfig.mq4Off);

        deviceConfig.mq135Enabled = prefs.getBool("mq135_en", deviceConfig.mq135Enabled);
        deviceConfig.mq135On = prefs.getInt("mq135_on", deviceConfig.mq135On);
        deviceConfig.mq135Off = prefs.getInt("mq135_off", deviceConfig.mq135Off);

        deviceConfig.dhtEnabled = prefs.getBool("dht_en", deviceConfig.dhtEnabled);
        deviceConfig.tempWarn = prefs.getFloat("temp_warn", deviceConfig.tempWarn);

        deviceConfig.buzzerEnabled = prefs.getBool("buzzer_en", deviceConfig.buzzerEnabled);

        prefs.end();
        LOGI(TAG, "Configuration loaded successfully from NVS");
    } else {
        LOGW(TAG, "Could not open NVS preferences, defaults used");
    }
}

void StorageManager::saveConfig(const Config::DeviceConfig& deviceConfig) {
    if (prefs.begin(Config::NVS_NAMESPACE, false)) {
        prefs.putBool("mq2_en", deviceConfig.mq2Enabled);
        prefs.putInt("mq2_on", deviceConfig.mq2On);
        prefs.putInt("mq2_off", deviceConfig.mq2Off);

        prefs.putBool("mq4_en", deviceConfig.mq4Enabled);
        prefs.putInt("mq4_on", deviceConfig.mq4On);
        prefs.putInt("mq4_off", deviceConfig.mq4Off);

        prefs.putBool("mq135_en", deviceConfig.mq135Enabled);
        prefs.putInt("mq135_on", deviceConfig.mq135On);
        prefs.putInt("mq135_off", deviceConfig.mq135Off);

        prefs.putBool("dht_en", deviceConfig.dhtEnabled);
        prefs.putFloat("temp_warn", deviceConfig.tempWarn);

        prefs.putBool("buzzer_en", deviceConfig.buzzerEnabled);

        prefs.end();
        LOGI(TAG, "Configuration written successfully to NVS");
    } else {
        LOGE(TAG, "Failed to open NVS preferences for writing");
    }
}
