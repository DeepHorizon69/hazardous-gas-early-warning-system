#include "SensorManager.h"
#include "Logger.h"

static const char* TAG = "SensorManager";
static DHT dht(Config::PIN_DHT22, DHT22);

SensorManager::SensorManager() :
    m_bootTime(0),
    m_heatingFinished(false),
    m_mq2Raw(0), m_mq4Raw(0), m_mq135Raw(0),
    m_mq2Filtered(0), m_mq4Filtered(0), m_mq135Filtered(0),
    m_temperature(NAN), m_humidity(NAN),
    m_mq2Fault(false), m_mq4Fault(false), m_mq135Fault(false), m_dhtFault(false),
    m_mq2FaultStart(0), m_mq4FaultStart(0), m_mq135FaultStart(0),
    m_lastDHTRead(0), m_dhtFailCount(0),
    m_mq2Ema(0.2f), m_mq4Ema(0.2f), m_mq135Ema(0.2f) // Alpha = 0.2
{}

void SensorManager::begin() {
    m_bootTime = millis();
    dht.begin();
    
    m_mq2Median.clear();
    m_mq4Median.clear();
    m_mq135Median.clear();
    m_mq2Ema.clear();
    m_mq4Ema.clear();
    m_mq135Ema.clear();

    analogReadResolution(12); // ESP32 12-bit ADC (0 - 4095)
    LOGI(TAG, "SensorManager initialized");
}

bool SensorManager::isHeating() const {
    return (millis() - m_bootTime) < Config::HEATING_DURATION_MS;
}

uint32_t SensorManager::getHeatingElapsed() const {
    uint32_t elapsed = millis() - m_bootTime;
    return elapsed > Config::HEATING_DURATION_MS ? Config::HEATING_DURATION_MS : elapsed;
}

uint32_t SensorManager::getHeatingRemaining() const {
    uint32_t elapsed = getHeatingElapsed();
    return (Config::HEATING_DURATION_MS - elapsed) / 1000UL;
}

uint8_t SensorManager::getHeatingProgress() const {
    uint32_t elapsed = getHeatingElapsed();
    return (elapsed * 100UL) / Config::HEATING_DURATION_MS;
}

void SensorManager::update(const Config::DeviceConfig& deviceConfig) {
    readMQSensors(deviceConfig);
    readDHTSensor(deviceConfig);
    diagnoseFaults(deviceConfig);
    compensateReadings();

    // Log heating completion once
    if (!isHeating() && !m_heatingFinished) {
        m_heatingFinished = true;
        LOGI(TAG, "Pre-heating completed. Sensor readings stable.");
    }
}

void SensorManager::readMQSensors(const Config::DeviceConfig& deviceConfig) {
    if (deviceConfig.mq2Enabled) {
        m_mq2Raw = analogRead(Config::PIN_MQ2);
        uint16_t med = m_mq2Median.update(m_mq2Raw);
        m_mq2Filtered = (uint16_t)m_mq2Ema.update(med);
    } else {
        m_mq2Raw = 0;
        m_mq2Filtered = 0;
    }

    if (deviceConfig.mq4Enabled) {
        m_mq4Raw = analogRead(Config::PIN_MQ4);
        uint16_t med = m_mq4Median.update(m_mq4Raw);
        m_mq4Filtered = (uint16_t)m_mq4Ema.update(med);
    } else {
        m_mq4Raw = 0;
        m_mq4Filtered = 0;
    }

    if (deviceConfig.mq135Enabled) {
        m_mq135Raw = analogRead(Config::PIN_MQ135);
        uint16_t med = m_mq135Median.update(m_mq135Raw);
        m_mq135Filtered = (uint16_t)m_mq135Ema.update(med);
    } else {
        m_mq135Raw = 0;
        m_mq135Filtered = 0;
    }
}

void SensorManager::readDHTSensor(const Config::DeviceConfig& deviceConfig) {
    if (!deviceConfig.dhtEnabled) {
        m_temperature = NAN;
        m_humidity = NAN;
        m_dhtFault = false;
        return;
    }

    // DHT22 cannot be queried faster than once every 2 seconds
    if (millis() - m_lastDHTRead < 2000UL) {
        return;
    }
    m_lastDHTRead = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (isnan(t) || isnan(h)) {
        m_dhtFailCount++;
        if (m_dhtFailCount >= 3) {
            if (!m_dhtFault) {
                LOGE(TAG, "DHT22 communication fault detected");
            }
            m_dhtFault = true;
            m_temperature = NAN;
            m_humidity = NAN;
        }
    } else {
        m_dhtFailCount = 0;
        m_dhtFault = false;
        m_temperature = t;
        m_humidity = h;
    }
}

static void checkADCBounds(uint16_t rawValue, bool& faultFlag, uint32_t& faultStart, const char* name) {
    // If ADC pin is floating, it reads random values. If disconnected, MQ sensors usually pull low.
    // Let's check for extreme limits (0V or 3.3V saturation)
    bool outOfBounds = (rawValue <= 5 || rawValue >= 4090);
    
    if (outOfBounds) {
        if (faultStart == 0) {
            faultStart = millis();
        }
        if (millis() - faultStart >= 10000UL) { // 10 seconds debounce
            if (!faultFlag) {
                LOGE(TAG, "%s ADC fault detected (Raw: %u)", name, rawValue);
            }
            faultFlag = true;
        }
    } else {
        faultStart = 0;
        faultFlag = false;
    }
}

void SensorManager::diagnoseFaults(const Config::DeviceConfig& deviceConfig) {
    if (deviceConfig.mq2Enabled) {
        checkADCBounds(m_mq2Raw, m_mq2Fault, m_mq2FaultStart, "MQ2");
    } else {
        m_mq2Fault = false;
    }

    if (deviceConfig.mq4Enabled) {
        checkADCBounds(m_mq4Raw, m_mq4Fault, m_mq4FaultStart, "MQ4");
    } else {
        m_mq4Fault = false;
    }

    if (deviceConfig.mq135Enabled) {
        checkADCBounds(m_mq135Raw, m_mq135Fault, m_mq135FaultStart, "MQ135");
    } else {
        m_mq135Fault = false;
    }
}

void SensorManager::compensateReadings() {
    // Only compensate if DHT readings are valid
    if (m_dhtFault || isnan(m_temperature) || isnan(m_humidity)) {
        return;
    }

    // Apply linear compensation offsets
    float tempDelta = m_temperature - 20.0f; // Reference temperature is 20°C
    float humDelta = m_humidity - 65.0f;     // Reference humidity is 65%RH
    
    float correction = 1.0f + (Config::TEMP_COMP_COEFF * tempDelta) + (Config::HUM_COMP_COEFF * humDelta);
    
    // Safety check on correction factor
    if (correction < 0.5f) correction = 0.5f;
    if (correction > 1.5f) correction = 1.5f;

    if (!m_mq2Fault && m_mq2Raw > 0) {
        float comp = m_mq2Filtered * correction;
        m_mq2Filtered = (uint16_t)constrain(comp, 0.0f, 4095.0f);
    }
    
    if (!m_mq4Fault && m_mq4Raw > 0) {
        float comp = m_mq4Filtered * correction;
        m_mq4Filtered = (uint16_t)constrain(comp, 0.0f, 4095.0f);
    }

    if (!m_mq135Fault && m_mq135Raw > 0) {
        float comp = m_mq135Filtered * correction;
        m_mq135Filtered = (uint16_t)constrain(comp, 0.0f, 4095.0f);
    }
}
