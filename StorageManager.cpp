#include "StorageManager.h"
#include "Logger.h"

static const char* TAG = "StorageManager";

StorageManager::StorageManager() {}

void StorageManager::begin() {
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
    loadDefaultConfig(deviceConfig);

    if (m_prefs.begin(Config::NVS_NAMESPACE, true)) {
        deviceConfig.mq2Enabled = m_prefs.getBool("mq2_en", deviceConfig.mq2Enabled);
        deviceConfig.mq2On = m_prefs.getInt("mq2_on", deviceConfig.mq2On);
        deviceConfig.mq2Off = m_prefs.getInt("mq2_off", deviceConfig.mq2Off);

        deviceConfig.mq4Enabled = m_prefs.getBool("mq4_en", deviceConfig.mq4Enabled);
        deviceConfig.mq4On = m_prefs.getInt("mq4_on", deviceConfig.mq4On);
        deviceConfig.mq4Off = m_prefs.getInt("mq4_off", deviceConfig.mq4Off);

        deviceConfig.mq135Enabled = m_prefs.getBool("mq135_en", deviceConfig.mq135Enabled);
        deviceConfig.mq135On = m_prefs.getInt("mq135_on", deviceConfig.mq135On);
        deviceConfig.mq135Off = m_prefs.getInt("mq135_off", deviceConfig.mq135Off);

        deviceConfig.dhtEnabled = m_prefs.getBool("dht_en", deviceConfig.dhtEnabled);
        deviceConfig.tempWarn = m_prefs.getFloat("temp_warn", deviceConfig.tempWarn);

        deviceConfig.buzzerEnabled = m_prefs.getBool("buzzer_en", deviceConfig.buzzerEnabled);

        m_prefs.end();
        LOGI(TAG, "Configuration loaded successfully from NVS");
    } else {
        LOGW(TAG, "Could not open NVS preferences, defaults used");
    }
}

void StorageManager::saveConfig(const Config::DeviceConfig& deviceConfig) {
    if (m_prefs.begin(Config::NVS_NAMESPACE, false)) {
        m_prefs.putBool("mq2_en", deviceConfig.mq2Enabled);
        m_prefs.putInt("mq2_on", deviceConfig.mq2On);
        m_prefs.putInt("mq2_off", deviceConfig.mq2Off);

        m_prefs.putBool("mq4_en", deviceConfig.mq4Enabled);
        m_prefs.putInt("mq4_on", deviceConfig.mq4On);
        m_prefs.putInt("mq4_off", deviceConfig.mq4Off);

        m_prefs.putBool("mq135_en", deviceConfig.mq135Enabled);
        m_prefs.putInt("mq135_on", deviceConfig.mq135On);
        m_prefs.putInt("mq135_off", deviceConfig.mq135Off);

        m_prefs.putBool("dht_en", deviceConfig.dhtEnabled);
        m_prefs.putFloat("temp_warn", deviceConfig.tempWarn);

        m_prefs.putBool("buzzer_en", deviceConfig.buzzerEnabled);

        m_prefs.end();
        LOGI(TAG, "Configuration written successfully to NVS");
    } else {
        LOGE(TAG, "Failed to open NVS preferences for writing");
    }
}
