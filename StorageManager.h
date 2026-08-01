#pragma once

#include "Config.h"

class StorageManager {
public:
    StorageManager();
    void begin();
    void loadConfig(Config::DeviceConfig& deviceConfig);
    void saveConfig(const Config::DeviceConfig& deviceConfig);
    void loadDefaultConfig(Config::DeviceConfig& deviceConfig);
};
