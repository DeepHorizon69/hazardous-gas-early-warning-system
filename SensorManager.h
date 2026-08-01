#pragma once

#include "Config.h"
#include "Filters.h"
#include <DHT.h>

class SensorManager {
public:
    SensorManager();
    void begin();

    void update(const Config::DeviceConfig& deviceConfig);

    uint16_t getMQ2Raw() const { return m_mq2Raw; }
    uint16_t getMQ2Filtered() const { return m_mq2Filtered; }
    uint16_t getMQ4Raw() const { return m_mq4Raw; }
    uint16_t getMQ4Filtered() const { return m_mq4Filtered; }
    uint16_t getMQ135Raw() const { return m_mq135Raw; }
    uint16_t getMQ135Filtered() const { return m_mq135Filtered; }

    float getTemperature() const { return m_temperature; }
    float getHumidity() const { return m_humidity; }

    bool hasMQ2Fault() const { return m_mq2Fault; }
    bool hasMQ4Fault() const { return m_mq4Fault; }
    bool hasMQ135Fault() const { return m_mq135Fault; }
    bool hasDHTFault() const { return m_dhtFault; }
    bool hasAnyFault() const { return m_mq2Fault || m_mq4Fault || m_mq135Fault || m_dhtFault; }

    bool isHeating() const;
    uint32_t getHeatingElapsed() const;
    uint32_t getHeatingRemaining() const;
    uint8_t getHeatingProgress() const;

private:
    void readMQSensors(const Config::DeviceConfig& deviceConfig);
    void readDHTSensor(const Config::DeviceConfig& deviceConfig);
    void diagnoseFaults(const Config::DeviceConfig& deviceConfig);
    void compensateReadings();

    void checkADCBounds(uint16_t rawValue, bool& faultFlag, uint32_t& faultStart, const char* name);

    DHT m_dht;

    uint32_t m_bootTime;
    bool m_heatingFinished;

    uint16_t m_mq2Raw;
    uint16_t m_mq4Raw;
    uint16_t m_mq135Raw;

    uint16_t m_mq2Filtered;
    uint16_t m_mq4Filtered;
    uint16_t m_mq135Filtered;

    float m_temperature;
    float m_humidity;

    bool m_mq2Fault;
    bool m_mq4Fault;
    bool m_mq135Fault;
    bool m_dhtFault;

    uint32_t m_mq2FaultStart;
    uint32_t m_mq4FaultStart;
    uint32_t m_mq135FaultStart;

    uint32_t m_lastDHTRead;
    uint8_t m_dhtFailCount;

    Filters::MedianFilter<uint16_t, 3> m_mq2Median;
    Filters::MedianFilter<uint16_t, 3> m_mq4Median;
    Filters::MedianFilter<uint16_t, 3> m_mq135Median;

    Filters::ExponentialMovingAverage m_mq2Ema;
    Filters::ExponentialMovingAverage m_mq4Ema;
    Filters::ExponentialMovingAverage m_mq135Ema;
};
