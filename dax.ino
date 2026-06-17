/************************************************************
 * EARLY HAZARDOUS GAS DETECTION SYSTEM
 * PART 1
 * Includes, Definitions, Globals, Config, Preferences,
 * Moving Average, OLED Base
 ************************************************************/

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

/************************************************************
 * WIFI
 ************************************************************/

const char* WIFI_SSID = "DAVINCI";
const char* WIFI_PASSWORD = "DUKEHORNP";

/************************************************************
 * MQTT
 ************************************************************/

const char* MQTT_SERVER = "broker.emqx.io";
const uint16_t MQTT_PORT = 1883;

const char* MQTT_NAMESPACE = "alsa";

char topicData[64];
char topicState[64];
char topicConfig[64];
char topicConfigAck[64];

const char* TOPIC_DATA = topicData;
const char* TOPIC_STATE = topicState;
const char* TOPIC_CONFIG = topicConfig;
const char* TOPIC_CONFIG_ACK = topicConfigAck;

void buildMQTTTopics()
{
  snprintf(topicData, sizeof(topicData), "%s/gas/data", MQTT_NAMESPACE);
  snprintf(topicState, sizeof(topicState), "%s/gas/state", MQTT_NAMESPACE);
  snprintf(topicConfig, sizeof(topicConfig), "%s/gas/config", MQTT_NAMESPACE);
  snprintf(topicConfigAck, sizeof(topicConfigAck), "%s/gas/config/ack", MQTT_NAMESPACE);
}

/************************************************************
 * HARDWARE
 ************************************************************/

#define PIN_MQ2       34
#define PIN_MQ4       35
#define PIN_MQ135     32

#define PIN_DHT22      4
#define PIN_BUZZER    26

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_ADDRESS 0x3C

#define DHTTYPE DHT22

/************************************************************
 * TIMING
 ************************************************************/

const uint32_t HEATING_DURATION_MS = 300000UL;

const uint32_t WIFI_RECONNECT_INTERVAL = 10000UL;
const uint32_t MQTT_RECONNECT_INTERVAL = 5000UL;
const uint32_t TELEMETRY_INTERVAL = 2000UL;
const uint32_t OLED_INTERVAL = 500UL;
const uint32_t FAULT_CHECK_INTERVAL = 1000UL;

const uint32_t MONITOR_PAGE_TIME = 5000UL;
const uint32_t NETWORK_PAGE_TIME = 3000UL;

/************************************************************
 * MOVING AVERAGE
 ************************************************************/

#define MOVING_AVG_SAMPLES 10

/************************************************************
 * STATE MACHINE
 ************************************************************/

enum SystemState
{
  STATE_BOOT,
  STATE_HEATING,
  STATE_NORMAL,
  STATE_WARNING,
  STATE_DANGER
};

enum OLEDPage
{
  PAGE_MONITOR,
  PAGE_NETWORK
};

/************************************************************
 * FORWARD DECLARATIONS
 ************************************************************/

const char* getStateText(SystemState state);

/************************************************************
 * CONFIG STRUCT
 ************************************************************/

struct Config
{
  bool mq2Enabled;
  int mq2On;
  int mq2Off;

  bool mq4Enabled;
  int mq4On;
  int mq4Off;

  bool mq135Enabled;
  int mq135On;
  int mq135Off;

  bool dhtEnabled;
  float tempWarn;

  bool buzzerEnabled;
};

/************************************************************
 * MOVING AVERAGE CLASS
 ************************************************************/

class MovingAverage
{
public:

  MovingAverage()
  {
    clear();
  }

  void clear()
  {
    sum = 0;
    index = 0;
    count = 0;

    for (uint8_t i = 0; i < MOVING_AVG_SAMPLES; i++)
    {
      buffer[i] = 0;
    }
  }

  uint16_t update(uint16_t value)
  {
    sum -= buffer[index];

    buffer[index] = value;

    sum += value;

    index++;

    if (index >= MOVING_AVG_SAMPLES)
    {
      index = 0;
    }

    if (count < MOVING_AVG_SAMPLES)
    {
      count++;
    }

    return sum / count;
  }

private:

  uint16_t buffer[MOVING_AVG_SAMPLES];
  uint32_t sum;
  uint8_t index;
  uint8_t count;
};

/************************************************************
 * GLOBAL OBJECTS
 ************************************************************/

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

Preferences preferences;

DHT dht(PIN_DHT22, DHTTYPE);

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  -1
);

/************************************************************
 * GLOBAL CONFIG
 ************************************************************/
void WiFiEvent(
  WiFiEvent_t event
)
{
  switch(event)
  {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:

      Serial.print(
        "[WiFi] IP: "
      );

      Serial.println(
        WiFi.localIP()
      );

      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:

      Serial.println(
        "[WiFi] Lost"
      );

      break;

    default:
      break;
  }
}

Config config;

/************************************************************
 * MOVING AVERAGE INSTANCES
 ************************************************************/

MovingAverage mq2Filter;
MovingAverage mq4Filter;
MovingAverage mq135Filter;

/************************************************************
 * SENSOR VALUES
 ************************************************************/

uint16_t mq2Raw = 0;
uint16_t mq4Raw = 0;
uint16_t mq135Raw = 0;

uint16_t mq2Avg = 0;
uint16_t mq4Avg = 0;
uint16_t mq135Avg = 0;

float temperature = NAN;
float humidity = NAN;

/************************************************************
 * FAULT FLAGS
 ************************************************************/

bool mq2Fault = false;
bool mq4Fault = false;
bool mq135Fault = false;
bool dht22Fault = false;

/************************************************************
 * FAULT TIMERS
 ************************************************************/

uint32_t mq2FaultStart = 0;
uint32_t mq4FaultStart = 0;
uint32_t mq135FaultStart = 0;

/************************************************************
 * SYSTEM
 ************************************************************/

SystemState currentState = STATE_BOOT;
SystemState previousState = STATE_BOOT;

OLEDPage currentPage = PAGE_MONITOR;

bool heatingFinished = false;

/************************************************************
 * TIMERS
 ************************************************************/

uint32_t bootTime = 0;

uint32_t lastWiFiReconnect = 0;
uint32_t lastMQTTReconnect = 0;

uint32_t lastTelemetry = 0;
uint32_t lastOLEDUpdate = 0;
uint32_t lastFaultCheck = 0;

uint32_t pageTimer = 0;

/************************************************************
 * OLED CACHE
 ************************************************************/

char oledCache[256] = "";
char oledBuffer[256] = "";

/************************************************************
 * STATE TEXT
 ************************************************************/

const char* getStateText(SystemState state)
{
  switch (state)
  {
    case STATE_BOOT:
      return "BOOT";

    case STATE_HEATING:
      return "HEATING";

    case STATE_NORMAL:
      return "NORMAL";

    case STATE_WARNING:
      return "WARNING";

    case STATE_DANGER:
      return "DANGER";

    default:
      return "UNKNOWN";
  }
}

/************************************************************
 * CONFIG DEFAULTS
 ************************************************************/

void loadDefaultConfig()
{
  config.mq2Enabled = true;
  config.mq2On = 2500;
  config.mq2Off = 1800;

  config.mq4Enabled = true;
  config.mq4On = 2500;
  config.mq4Off = 1800;

  config.mq135Enabled = true;
  config.mq135On = 2500;
  config.mq135Off = 1800;

  config.dhtEnabled = true;
  config.tempWarn = 40.0f;

  config.buzzerEnabled = true;
}

/************************************************************
 * SAVE CONFIG
 ************************************************************/

void saveConfig()
{
  preferences.begin("gascfg", false);

  preferences.putBool("mq2_en", config.mq2Enabled);
  preferences.putInt("mq2_on", config.mq2On);
  preferences.putInt("mq2_off", config.mq2Off);

  preferences.putBool("mq4_en", config.mq4Enabled);
  preferences.putInt("mq4_on", config.mq4On);
  preferences.putInt("mq4_off", config.mq4Off);

  preferences.putBool("mq135_en", config.mq135Enabled);
  preferences.putInt("mq135_on", config.mq135On);
  preferences.putInt("mq135_off", config.mq135Off);

  preferences.putBool("dht_en", config.dhtEnabled);
  preferences.putFloat("temp_warn", config.tempWarn);

  preferences.putBool("buzzer_en", config.buzzerEnabled);

  preferences.end();

  Serial.println("[Config] Saved");
}

/************************************************************
 * LOAD CONFIG
 ************************************************************/

void loadConfig()
{
  loadDefaultConfig();

  preferences.begin("gascfg", true);

  config.mq2Enabled =
    preferences.getBool("mq2_en",
    config.mq2Enabled);

  config.mq2On =
    preferences.getInt("mq2_on",
    config.mq2On);

  config.mq2Off =
    preferences.getInt("mq2_off",
    config.mq2Off);

  config.mq4Enabled =
    preferences.getBool("mq4_en",
    config.mq4Enabled);

  config.mq4On =
    preferences.getInt("mq4_on",
    config.mq4On);

  config.mq4Off =
    preferences.getInt("mq4_off",
    config.mq4Off);

  config.mq135Enabled =
    preferences.getBool("mq135_en",
    config.mq135Enabled);

  config.mq135On =
    preferences.getInt("mq135_on",
    config.mq135On);

  config.mq135Off =
    preferences.getInt("mq135_off",
    config.mq135Off);

  config.dhtEnabled =
    preferences.getBool("dht_en",
    config.dhtEnabled);

  config.tempWarn =
    preferences.getFloat("temp_warn",
    config.tempWarn);

  config.buzzerEnabled =
    preferences.getBool("buzzer_en",
    config.buzzerEnabled);

  preferences.end();

  Serial.println("[Config] Loaded");
}

/************************************************************
 * OLED BASIC DRAW
 ************************************************************/

void oledPrint(const char* text)
{
  if (strcmp(text, oledCache) == 0)
  {
    return;
  }

  strncpy(
    oledCache,
    text,
    sizeof(oledCache) - 1
  );

  display.clearDisplay();

  display.setCursor(0, 0);
  display.print(text);

  display.display();
}

/************************************************************
 * BOOT SCREEN
 ************************************************************/

void showBootScreen()
{
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(8, 10);
  display.println("Gas");

  display.setCursor(8, 35);
  display.println("Monitor");

  display.setTextSize(1);

  display.setCursor(18, 58);
  display.println("Booting...");

  display.display();
}

/************************************************************
 * PART 2
 * WiFi, MQTT, Config Validation,
 * MQTT Callback, Config ACK,
 * Reconnect Manager
 ************************************************************/

/************************************************************
 * FORWARD DECLARATIONS
 ************************************************************/

bool validateConfigJson(StaticJsonDocument<1024>& doc);
void publishConfigAck();
void publishState();
void mqttCallback(char* topic, byte* payload, unsigned int length);

/************************************************************
 * WIFI
 ************************************************************/

void connectWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  Serial.println("[WiFi]");
  Serial.println("Connecting");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void maintainWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  if (millis() - lastWiFiReconnect <
      WIFI_RECONNECT_INTERVAL)
  {
    return;
  }

  lastWiFiReconnect = millis();

  Serial.println("[WiFi]");
  Serial.println("Reconnect");

  WiFi.disconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

bool wifiConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

/************************************************************
 * MQTT CLIENT ID
 ************************************************************/

void buildClientId(char* buffer, size_t len)
{
  uint64_t chipid = ESP.getEfuseMac();

  snprintf(
    buffer,
    len,
    "ESP32-GAS-%04X",
    (uint16_t)(chipid & 0xFFFF)
  );
}

/************************************************************
 * MQTT SUBSCRIBE
 ************************************************************/

void subscribeTopics()
{
  static bool subscribed = false;

  if (!mqttClient.connected())
  {
    subscribed = false;
    return;
  }

  if (subscribed)
  {
    return;
  }

  mqttClient.subscribe(TOPIC_CONFIG);

  subscribed = true;

  Serial.println("[MQTT]");
  Serial.println("Subscribed");
}

/************************************************************
 * MQTT CONNECT
 ************************************************************/

bool connectMQTT()
{
  if (!wifiConnected())
  {
    return false;
  }

  if (mqttClient.connected())
  {
    return true;
  }

  char clientId[32];

  buildClientId(clientId, sizeof(clientId));

  Serial.println("[MQTT]");
  Serial.println("Connecting");

  bool ok = mqttClient.connect(clientId);

  if (!ok)
  {
    Serial.print("[MQTT] Failed rc=");
    Serial.println(mqttClient.state());
    return false;
  }

  Serial.println("[MQTT]");
  Serial.println("Connected");

  subscribeTopics();

  publishConfigAck();

  return true;
}

/************************************************************
 * MQTT MAINTAIN
 ************************************************************/

void maintainMQTT()
{
  if (!wifiConnected())
  {
    return;
  }

  if (mqttClient.connected())
  {
    mqttClient.loop();
    return;
  }

  if (millis() - lastMQTTReconnect <
      MQTT_RECONNECT_INTERVAL)
  {
    return;
  }

  lastMQTTReconnect = millis();

  connectMQTT();
}

/************************************************************
 * CONFIG VALIDATION HELPERS
 ************************************************************/

bool isValidThreshold(JsonVariant value)
{
  if (!value.is<int>())
  {
    return false;
  }

  int v = value.as<int>();

  return v >= 0;
}

bool isValidBool(JsonVariant value)
{
  return value.is<bool>();
}

bool isValidFloat(JsonVariant value)
{
  return value.is<float>() ||
         value.is<double>() ||
         value.is<int>();
}

/************************************************************
 * CONFIG VALIDATION
 ************************************************************/

bool validateConfigJson(
  StaticJsonDocument<1024>& doc
)
{
  if (!doc.containsKey("mq2"))
  {
    Serial.println("[Config] Missing mq2");
    return false;
  }

  if (!doc.containsKey("mq4"))
  {
    Serial.println("[Config] Missing mq4");
    return false;
  }

  if (!doc.containsKey("mq135"))
  {
    Serial.println("[Config] Missing mq135");
    return false;
  }

  if (!doc.containsKey("dht22"))
  {
    Serial.println("[Config] Missing dht22");
    return false;
  }

  if (!doc.containsKey("buzzerEnabled"))
  {
    Serial.println("[Config] Missing buzzerEnabled");
    return false;
  }

  JsonObject mq2 = doc["mq2"];
  JsonObject mq4 = doc["mq4"];
  JsonObject mq135 = doc["mq135"];
  JsonObject dht22 = doc["dht22"];

  if (!isValidBool(mq2["enabled"])) return false;
  if (!isValidThreshold(mq2["on"])) return false;
  if (!isValidThreshold(mq2["off"])) return false;

  if (!isValidBool(mq4["enabled"])) return false;
  if (!isValidThreshold(mq4["on"])) return false;
  if (!isValidThreshold(mq4["off"])) return false;

  if (!isValidBool(mq135["enabled"])) return false;
  if (!isValidThreshold(mq135["on"])) return false;
  if (!isValidThreshold(mq135["off"])) return false;

  if (!isValidBool(dht22["enabled"])) return false;
  if (!isValidFloat(dht22["tempWarn"])) return false;

  if (!isValidBool(doc["buzzerEnabled"]))
  {
    return false;
  }

  if ((int)mq2["on"] < (int)mq2["off"])
  {
    return false;
  }

  if ((int)mq4["on"] < (int)mq4["off"])
  {
    return false;
  }

  if ((int)mq135["on"] < (int)mq135["off"])
  {
    return false;
  }

  return true;
}

/************************************************************
 * APPLY CONFIG
 ************************************************************/

void applyConfig(
  StaticJsonDocument<1024>& doc
)
{
  JsonObject mq2 = doc["mq2"];
  JsonObject mq4 = doc["mq4"];
  JsonObject mq135 = doc["mq135"];
  JsonObject dht22 = doc["dht22"];

  config.mq2Enabled = mq2["enabled"];
  config.mq2On = mq2["on"];
  config.mq2Off = mq2["off"];

  config.mq4Enabled = mq4["enabled"];
  config.mq4On = mq4["on"];
  config.mq4Off = mq4["off"];

  config.mq135Enabled = mq135["enabled"];
  config.mq135On = mq135["on"];
  config.mq135Off = mq135["off"];

  config.dhtEnabled = dht22["enabled"];
  config.tempWarn = dht22["tempWarn"];

  config.buzzerEnabled =
    doc["buzzerEnabled"];

  saveConfig();

  publishConfigAck();

  Serial.println("[Config]");
  Serial.println("Applied");
}

/************************************************************
 * CONFIG ACK
 ************************************************************/

void publishConfigAck()
{
  if (!mqttClient.connected())
  {
    return;
  }

  StaticJsonDocument<512> doc;

  JsonObject mq2 = doc.createNestedObject("mq2");
  mq2["enabled"] = config.mq2Enabled;
  mq2["on"] = config.mq2On;
  mq2["off"] = config.mq2Off;

  JsonObject mq4 = doc.createNestedObject("mq4");
  mq4["enabled"] = config.mq4Enabled;
  mq4["on"] = config.mq4On;
  mq4["off"] = config.mq4Off;

  JsonObject mq135 = doc.createNestedObject("mq135");
  mq135["enabled"] = config.mq135Enabled;
  mq135["on"] = config.mq135On;
  mq135["off"] = config.mq135Off;

  JsonObject dht22 =
    doc.createNestedObject("dht22");

  dht22["enabled"] = config.dhtEnabled;
  dht22["tempWarn"] = config.tempWarn;

  doc["buzzerEnabled"] =
    config.buzzerEnabled;

  char payload[768];

  size_t len =
    serializeJson(
      doc,
      payload,
      sizeof(payload)
    );

  mqttClient.publish(
    TOPIC_CONFIG_ACK,
    payload,
    len
  );

  Serial.println("[Config]");
  Serial.println("ACK Published");
}

/************************************************************
 * MQTT CONFIG HANDLER
 ************************************************************/

void processConfigPayload(
  const byte* payload,
  unsigned int length)
{
  if (length == 0)
  {
    return;
  }

  StaticJsonDocument<1024> doc;

  DeserializationError err =
    deserializeJson(
      doc,
      payload,
      length
    );

  if (err)
  {
    Serial.println("[Config]");
    Serial.println("Invalid JSON");
    return;
  }

  if (!validateConfigJson(doc))
  {
    Serial.println("[Config]");
    Serial.println("Validation Failed");
    return;
  }

  applyConfig(doc);
}

/************************************************************
 * MQTT CALLBACK
 ************************************************************/

void mqttCallback(
  char* topic,
  byte* payload,
  unsigned int length)
{
  Serial.println("[MQTT]");
  Serial.println("Message Received");

  if (strcmp(topic, TOPIC_CONFIG) == 0)
  {
    Serial.println("[Config]");
    Serial.println("Received");

    processConfigPayload(
      payload,
      length
    );
  }
}

/************************************************************
 * MQTT INIT
 ************************************************************/

void setupMQTT()
{
  mqttClient.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );

  mqttClient.setCallback(
    mqttCallback
  );

  mqttClient.setBufferSize(2048);
}

/************************************************************
 * NETWORK STATUS HELPERS
 ************************************************************/

const char* wifiStatusText()
{
  return wifiConnected()
         ? "OK"
         : "LOST";
}

const char* mqttStatusText()
{
  return mqttClient.connected()
         ? "OK"
         : "LOST";
}

/************************************************************
 * MQTT STATE PUBLISH
 ************************************************************/

void publishState()
{
  if (!mqttClient.connected())
  {
    return;
  }

  mqttClient.publish(
    TOPIC_STATE,
    getStateText(currentState),
    true
  );
}

/************************************************************
 * CONNECTION WATCHDOG
 ************************************************************/

void networkWatchdog()
{
  maintainWiFi();
  maintainMQTT();
  subscribeTopics();
}

/************************************************************
 * PART 2 END
 ************************************************************/
 /************************************************************
 * PART 3
 * Sensor Engine
 * DHT22
 * Fault Detection
 * Heating Logic
 * State Machine
 * Buzzer Manager
 ************************************************************/

/************************************************************
 * FORWARD DECLARATIONS
 ************************************************************/

/************************************************************
 * SENSOR READING
 ************************************************************/

void readMQSensors()
{
  mq2Raw = analogRead(PIN_MQ2);
  mq4Raw = analogRead(PIN_MQ4);
  mq135Raw = analogRead(PIN_MQ135);

  mq2Avg = mq2Filter.update(mq2Raw);
  mq4Avg = mq4Filter.update(mq4Raw);
  mq135Avg = mq135Filter.update(mq135Raw);
}

void readDHTSensor()
{
  static uint8_t failCount = 0;
  static uint32_t lastDHTRead = 0;

  if (!config.dhtEnabled)
  {
    return;
  }

  if (millis() - lastDHTRead < 2000UL)
  {
    return;
  }

  lastDHTRead = millis();

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h))
  {
    failCount++;

    temperature = NAN;
    humidity = NAN;

    if (failCount >= 3)
    {
      if (!dht22Fault)
      {
        Serial.println(
          "[Fault] DHT22 detected"
        );
      }

      dht22Fault = true;
    }

    return;
  }

  failCount = 0;

  dht22Fault = false;

  temperature = t;
  humidity = h;
}

/************************************************************
 * SENSOR UPDATE
 ************************************************************/

void updateSensors()
{
  readMQSensors();
  readDHTSensor();
}

/************************************************************
 * FAULT HELPERS
 ************************************************************/

bool invalidADC(uint16_t value)
{
  return (
    value <= 5 ||
    value >= 4090
  );
}

void processADCFault(
  uint16_t value,
  bool &faultFlag,
  uint32_t &faultStart,
  const char* name)
{
  if (invalidADC(value))
  {
    if (faultStart == 0)
    {
      faultStart = millis();
    }

    if ((millis() - faultStart) >= 10000UL)
    {
      if (!faultFlag)
      {
        Serial.print("[Fault] ");
        Serial.print(name);
        Serial.println(" detected");
      }

      faultFlag = true;
    }
  }
  else
  {
    faultStart = 0;
    faultFlag = false;
  }
}

/************************************************************
 * FAULT DETECTION
 ************************************************************/

void checkFaults()
{
  if (millis() - lastFaultCheck <
      FAULT_CHECK_INTERVAL)
  {
    return;
  }

  lastFaultCheck = millis();

  processADCFault(
    mq2Raw,
    mq2Fault,
    mq2FaultStart,
    "MQ2"
  );

  processADCFault(
    mq4Raw,
    mq4Fault,
    mq4FaultStart,
    "MQ4"
  );

  processADCFault(
    mq135Raw,
    mq135Fault,
    mq135FaultStart,
    "MQ135"
  );

  if (config.dhtEnabled)
  {
    if (isnan(temperature) ||
        isnan(humidity))
    {
      dht22Fault = true;
    }
  }
}

/************************************************************
 * FAULT EXIST
 ************************************************************/

bool hasFault()
{
  return
    mq2Fault ||
    mq4Fault ||
    mq135Fault ||
    dht22Fault;
}

/************************************************************
 * HEATING LOGIC
 ************************************************************/
void publishStatePeriodic()
{
  static uint32_t lastStatePub = 0;

  if (!mqttClient.connected())
  {
    return;
  }

  if (millis() - lastStatePub < 2000)
  {
    return;
  }

  lastStatePub = millis();

  mqttClient.publish(
    TOPIC_STATE,
    getStateText(currentState),
    true
  );
}

uint32_t getHeatingElapsed()
{
  uint32_t elapsed =
    millis() - bootTime;

  if (elapsed > HEATING_DURATION_MS)
  {
    elapsed = HEATING_DURATION_MS;
  }

  return elapsed;
}

uint32_t getHeatingRemaining()
{
  uint32_t elapsed =
    getHeatingElapsed();

  if (elapsed >= HEATING_DURATION_MS)
  {
    return 0;
  }

  return
    (HEATING_DURATION_MS - elapsed)
    / 1000UL;
}

uint8_t getHeatingProgress()
{
  uint32_t elapsed =
    getHeatingElapsed();

  return
    (elapsed * 100UL)
    / HEATING_DURATION_MS;
}

bool isHeatingMode()
{
  return
    (millis() - bootTime)
    < HEATING_DURATION_MS;
}

void updateHeatingState()
{
  if (isHeatingMode())
  {
    currentState = STATE_HEATING;
    return;
  }

  if (!heatingFinished)
  {
    heatingFinished = true;

    Serial.println("[System]");
    Serial.println("Heating Complete");
  }
}

/************************************************************
 * STATE HELPERS
 ************************************************************/

bool mq2Warning()
{
  return config.mq2Enabled &&
         !mq2Fault &&
         mq2Avg >= config.mq2Off;
}

bool mq4Warning()
{
  return config.mq4Enabled &&
         !mq4Fault &&
         mq4Avg >= config.mq4Off;
}

bool mq135Warning()
{
  return config.mq135Enabled &&
         !mq135Fault &&
         mq135Avg >= config.mq135Off;
}

bool mq2Danger()
{
  return config.mq2Enabled &&
         !mq2Fault &&
         mq2Avg >= config.mq2On;
}

bool mq4Danger()
{
  return config.mq4Enabled &&
         !mq4Fault &&
         mq4Avg >= config.mq4On;
}

bool mq135Danger()
{
  return config.mq135Enabled &&
         !mq135Fault &&
         mq135Avg >= config.mq135On;
}

/************************************************************
 * STATE MACHINE
 ************************************************************/

void evaluateState()
{
  if (isHeatingMode())
  {
    currentState = STATE_HEATING;
    return;
  }

  bool warning = false;
  bool danger = false;

  if (mq2Danger()) danger = true;
  if (mq4Danger()) danger = true;
  if (mq135Danger()) danger = true;

  if (config.dhtEnabled &&
      !dht22Fault)
  {
    if (temperature > 50.0f)
    {
      danger = true;
    }
  }

  if (danger)
  {
    currentState = STATE_DANGER;
    return;
  }

  if (mq2Warning()) warning = true;
  if (mq4Warning()) warning = true;
  if (mq135Warning()) warning = true;

  if (config.dhtEnabled &&
      !dht22Fault)
  {
    if (temperature > config.tempWarn)
    {
      warning = true;
    }
  }

  if (warning)
  {
    currentState = STATE_WARNING;
    return;
  }

  currentState = STATE_NORMAL;
}

/************************************************************
 * STATE TRANSITION
 ************************************************************/

void processStateMachine()
{
  previousState = currentState;

  evaluateState();

  if (previousState != currentState)
  {
    Serial.print("[State] ");
    Serial.print(
      getStateText(previousState)
    );

    Serial.print(" -> ");

    Serial.println(
      getStateText(currentState)
    );

    publishState();
  }
}

/************************************************************
 * BUZZER ENGINE
 ************************************************************/

bool buzzerState = false;
uint32_t buzzerTimer = 0;

void buzzerOff()
{
  digitalWrite(PIN_BUZZER, LOW);
  buzzerState = false;
}

void buzzerOn()
{
  digitalWrite(PIN_BUZZER, HIGH);
  buzzerState = true;
}

/************************************************************
 * WARNING BUZZER
 ************************************************************/

void updateWarningBuzzer()
{
  uint32_t now = millis();

  if (!buzzerState)
  {
    if (now - buzzerTimer >= 800)
    {
      buzzerOn();
      buzzerTimer = now;
    }
  }
  else
  {
    if (now - buzzerTimer >= 200)
    {
      buzzerOff();
      buzzerTimer = now;
    }
  }
}

/************************************************************
 * BUZZER MANAGER
 ************************************************************/

void updateBuzzer()
{
  if (!config.buzzerEnabled)
  {
    buzzerOff();
    return;
  }

  if (currentState == STATE_HEATING)
  {
    buzzerOff();
    return;
  }

  switch (currentState)
  {
    case STATE_NORMAL:
    {
      buzzerOff();
      break;
    }

    case STATE_WARNING:
    {
      updateWarningBuzzer();
      break;
    }

    case STATE_DANGER:
    {
      buzzerOn();
      break;
    }

    default:
    {
      buzzerOff();
      break;
    }
  }
}

/************************************************************
 * SENSOR DEBUG
 ************************************************************/

void printSensorDebug()
{
  static uint32_t lastPrint = 0;

  if (millis() - lastPrint < 5000)
  {
    return;
  }

  lastPrint = millis();

  Serial.print("[MQ2] ");
  Serial.println(mq2Avg);

  Serial.print("[MQ4] ");
  Serial.println(mq4Avg);

  Serial.print("[MQ135] ");
  Serial.println(mq135Avg);

  Serial.print("[Temp] ");
  Serial.println(temperature);

  Serial.print("[Hum] ");
  Serial.println(humidity);
}

/************************************************************
 * PART 3 END
 ************************************************************/
 /************************************************************
 * PART 4
 * OLED UI Engine
 * Heating Screen
 * Monitor Screen
 * Network Screen
 * Fault Screen
 * Page Rotation
 * Anti Flicker
 ************************************************************/

/************************************************************
 * OLED HELPERS
 ************************************************************/

void oledBeginFrame()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
}

void oledEndFrame()
{
  display.display();
}

bool oledChanged(const char* newContent)
{
  if (strcmp(oledCache, newContent) == 0)
  {
    return false;
  }

  strncpy(
    oledCache,
    newContent,
    sizeof(oledCache) - 1
  );

  oledCache[sizeof(oledCache) - 1] = '\0';

  return true;
}

/************************************************************
 * PROGRESS BAR
 ************************************************************/

void drawProgressBar(
  int x,
  int y,
  int w,
  int h,
  uint8_t progress)
{
  display.drawRect(x, y, w, h, SSD1306_WHITE);

  int fill =
    ((w - 2) * progress) / 100;

  display.fillRect(
    x + 1,
    y + 1,
    fill,
    h - 2,
    SSD1306_WHITE
  );
}

/************************************************************
 * HEATING SCREEN
 ************************************************************/

void drawHeatingScreen()
{
  uint32_t remain =
    getHeatingRemaining();

  uint8_t progress =
    getHeatingProgress();

  uint8_t min =
    remain / 60;

  uint8_t sec =
    remain % 60;

  snprintf(
    oledBuffer,
    sizeof(oledBuffer),
    "HEAT_%u_%u",
    remain,
    progress
  );

  if (!oledChanged(oledBuffer))
  {
    return;
  }

  oledBeginFrame();

  display.setTextSize(2);

  display.setCursor(10, 0);
  display.println("HEATING");

  char timeStr[16];

  snprintf(
    timeStr,
    sizeof(timeStr),
    "%02u:%02u",
    min,
    sec
  );

  display.setCursor(20, 25);
  display.println(timeStr);

  drawProgressBar(
    5,
    55,
    118,
    8,
    progress
  );

  oledEndFrame();
}

/************************************************************
 * MONITOR SCREEN
 ************************************************************/

void drawMonitorScreen()
{
  snprintf(
    oledBuffer,
    sizeof(oledBuffer),
    "MON_%u_%u_%u_%s",
    mq2Avg,
    mq4Avg,
    mq135Avg,
    getStateText(currentState)
  );

  if (!oledChanged(oledBuffer))
  {
    return;
  }

  oledBeginFrame();

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("MQ2  : ");
  display.println(mq2Avg);

  display.setCursor(0, 16);
  display.print("MQ4  : ");
  display.println(mq4Avg);

  display.setCursor(0, 32);
  display.print("MQ135: ");
  display.println(mq135Avg);

  display.drawLine(
    0,
    48,
    127,
    48,
    SSD1306_WHITE
  );

  display.setTextSize(2);

  display.setCursor(0, 52);
  display.println(
    getStateText(currentState)
  );

  oledEndFrame();
}

/************************************************************
 * NETWORK SCREEN
 ************************************************************/

void drawNetworkScreen()
{
  float t =
    isnan(temperature)
    ? 0.0f
    : temperature;

  float h =
    isnan(humidity)
    ? 0.0f
    : humidity;

  snprintf(
    oledBuffer,
    sizeof(oledBuffer),
    "NET_%.1f_%.1f_%s_%s",
    t,
    h,
    wifiStatusText(),
    mqttStatusText()
  );

  if (!oledChanged(oledBuffer))
  {
    return;
  }

  oledBeginFrame();

  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("Temp : ");

  if (dht22Fault)
  {
    display.println("--");
  }
  else
  {
    display.println(temperature, 1);
  }

  display.setCursor(0, 16);
  display.print("Hum  : ");

  if (dht22Fault)
  {
    display.println("--");
  }
  else
  {
    display.println(humidity, 1);
  }

  display.setCursor(0, 36);
  display.print("WiFi : ");
  display.println(
    wifiStatusText()
  );

  display.setCursor(0, 52);
  display.print("MQTT : ");
  display.println(
    mqttStatusText()
  );

  oledEndFrame();
}
/************************************************************
 * FAULT SCREEN
 ************************************************************/

const char* getFaultName()
{
  if (mq2Fault)
  {
    return "MQ2";
  }

  if (mq4Fault)
  {
    return "MQ4";
  }

  if (mq135Fault)
  {
    return "MQ135";
  }

  if (dht22Fault)
  {
    return "DHT22";
  }

  return "NONE";
}

void drawFaultScreen()
{
  snprintf(
    oledBuffer,
    sizeof(oledBuffer),
    "FAULT_%s",
    getFaultName()
  );

  if (!oledChanged(oledBuffer))
  {
    return;
  }

  oledBeginFrame();

  display.setTextSize(2);

  display.setCursor(10, 0);
  display.println("FAULT");

  display.setTextSize(2);

  display.setCursor(0, 30);
  display.println(
    getFaultName()
  );

  oledEndFrame();
}

/************************************************************
 * PAGE ROTATION
 ************************************************************/

void rotateOLEDPage()
{
  if (hasFault())
  {
    return;
  }

  if (currentState == STATE_HEATING)
  {
    return;
  }

  uint32_t duration =
    (currentPage == PAGE_MONITOR)
    ? MONITOR_PAGE_TIME
    : NETWORK_PAGE_TIME;

  if (millis() - pageTimer < duration)
  {
    return;
  }

  pageTimer = millis();

  if (currentPage == PAGE_MONITOR)
  {
    currentPage = PAGE_NETWORK;
  }
  else
  {
    currentPage = PAGE_MONITOR;
  }
}

/************************************************************
 * OLED ROUTER
 ************************************************************/

void updateOLED()
{
  if (millis() - lastOLEDUpdate <
      OLED_INTERVAL)
  {
    return;
  }

  lastOLEDUpdate = millis();

  rotateOLEDPage();

  if (currentState == STATE_HEATING)
  {
    drawHeatingScreen();
    return;
  }

  if (hasFault())
  {
    drawFaultScreen();
    return;
  }

  switch (currentPage)
  {
    case PAGE_MONITOR:
    {
      drawMonitorScreen();
      break;
    }

    case PAGE_NETWORK:
    {
      drawNetworkScreen();
      break;
    }

    default:
    {
      drawMonitorScreen();
      break;
    }
  }
}

/************************************************************
 * OLED INITIALIZATION
 ************************************************************/

void initOLED()
{
  if (!display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDRESS))
  {
    Serial.println(
      "[OLED] Init Failed"
    );

    return;
  }

  display.clearDisplay();
  display.setTextColor(
    SSD1306_WHITE
  );

  display.setTextSize(1);

  display.display();

  Serial.println(
    "[OLED] Ready"
  );
}

/************************************************************
 * FORCE OLED REFRESH
 ************************************************************/

void invalidateOLED()
{
  oledCache[0] = '\0';
}

/************************************************************
 * PART 4 END
 ************************************************************/
 /************************************************************
 * PART 5
 * Telemetry
 * MQTT Publish
 * Setup
 * Loop
 * Final Integration
 ************************************************************/

/************************************************************
 * TELEMETRY
 ************************************************************/

void publishTelemetry()
{
  if (!mqttClient.connected())
  {
    return;
  }

  if (millis() - lastTelemetry <
      TELEMETRY_INTERVAL)
  {
    return;
  }

  lastTelemetry = millis();

  StaticJsonDocument<2048> doc;

  if (mq2Fault)
  {
    doc["mq2"] = nullptr;
  }
  else
  {
    doc["mq2"] = mq2Avg;
  }

  if (mq4Fault)
  {
    doc["mq4"] = nullptr;
  }
  else
  {
    doc["mq4"] = mq4Avg;
  }

  if (mq135Fault)
  {
    doc["mq135"] = nullptr;
  }
  else
  {
    doc["mq135"] = mq135Avg;
  }

  if (isnan(temperature))
  {
    doc["temp"] = nullptr;
  }
  else
  {
    doc["temp"] = temperature;
  }

  if (isnan(humidity))
  {
    doc["hum"] = nullptr;
  }
  else
  {
    doc["hum"] = humidity;
  }

  doc["state"] =
    getStateText(currentState);

  doc["mq2Enabled"] =
    config.mq2Enabled;

  doc["mq4Enabled"] =
    config.mq4Enabled;

  doc["mq135Enabled"] =
    config.mq135Enabled;

  doc["dhtEnabled"] =
    config.dhtEnabled;

  doc["mq2Fault"] =
    mq2Fault;

  doc["mq4Fault"] =
    mq4Fault;

  doc["mq135Fault"] =
    mq135Fault;

  doc["dht22Fault"] =
    dht22Fault;

  doc["mq2On"] =
    config.mq2On;

  doc["mq2Off"] =
    config.mq2Off;

  doc["mq4On"] =
    config.mq4On;

  doc["mq4Off"] =
    config.mq4Off;

  doc["mq135On"] =
    config.mq135On;

  doc["mq135Off"] =
    config.mq135Off;

  doc["tempWarn"] =
    config.tempWarn;

  doc["heatingProgress"] =
    getHeatingProgress();

  doc["heatingRemaining"] =
    getHeatingRemaining();

  doc["heatingDuration"] =
    300;

  char payload[2048];

  size_t len =
    serializeJson(
      doc,
      payload,
      sizeof(payload)
    );

  mqttClient.publish(
    TOPIC_DATA,
    payload,
    len
  );
}
/************************************************************
 * HARDWARE INIT
 ************************************************************/

void initPins()
{
  pinMode(PIN_BUZZER, OUTPUT);

  digitalWrite(
    PIN_BUZZER,
    LOW
  );

  analogReadResolution(12);
}

/************************************************************
 * SENSOR INIT
 ************************************************************/

void initSensors()
{
  dht.begin();

  mq2Filter.clear();
  mq4Filter.clear();
  mq135Filter.clear();

  Serial.println(
    "[Sensor] Ready"
  );
}

/************************************************************
 * BOOT MQTT ACK
 ************************************************************/

void publishBootAck()
{
  if (!mqttClient.connected())
  {
    return;
  }

  publishConfigAck();
}

/************************************************************
 * SYSTEM INIT
 ************************************************************/

void initializeSystem()
{
  bootTime = millis();

  currentState = STATE_BOOT;
  previousState = STATE_BOOT;

  pageTimer = millis();

  invalidateOLED();

  Serial.println(
    "[System] Init Complete"
  );
}

/************************************************************
 * SETUP
 ************************************************************/

void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println(
    "================================"
  );
  Serial.println(
    "EARLY GAS DETECTION SYSTEM"
  );
  Serial.println(
    "================================"
  );

  initPins();

  Wire.begin(21, 22);

  initOLED();

  showBootScreen();

  loadConfig();

  WiFi.onEvent(WiFiEvent);

  initSensors();

  buildMQTTTopics();

  setupMQTT();

  connectWiFi();

  initializeSystem();

  delay(1500);

  currentState =
    STATE_HEATING;

  Serial.println(
    "[System] Boot Complete"
  );
}

/************************************************************
 * PERIODIC TASKS
 ************************************************************/

void updateSystem()
{
  updateSensors();

  checkFaults();

  updateHeatingState();

  processStateMachine();

  updateBuzzer();

  updateOLED();

  publishTelemetry();

  publishStatePeriodic();

  printSensorDebug();
}

/************************************************************
 * MAIN LOOP
 ************************************************************/

void loop()
{
  networkWatchdog();

  updateSystem();

  if (mqttClient.connected())
  {
    mqttClient.loop();
  }
}

/************************************************************
 * FINAL FIRMWARE END
 ************************************************************/
