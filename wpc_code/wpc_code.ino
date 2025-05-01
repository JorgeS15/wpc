#include <math.h>
#include <Adafruit_ADS1X15.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "LittleFS.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

// Wi-Fi credentials
const char* SSID = "WIFI_SSID";
const char* PASSWORD = "WIFI_PASSWORD";

// MQTT configuration
const char* MQTT_SERVER = "MQTT_IP";
const int MQTT_PORT = 1883;
const char* MQTT_USER = "USER";
const char* MQTT_PASSWORD = "PASSWORD";
const char* DEVICE_NAME = "wpc";
const char* DEVICE_ID = "wpc1";
const char* DEVICE_MANUFACTURER = "JorgeS15";
const char* DEVICE_MODEL = "Water Pressure Controller";
const char* DEVICE_VERSION = "2.0";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;

// Pin definitions
#define MOTOR_PIN 19
#define LED_RED 16
#define LED_GREEN 17
#define LED_BLUE 18
#define FLOW_PIN 23
#define TEMP_PIN 35
#define ledPin 2

// Constants
const int NUM_SAMPLES = 10;
const int DELAY = 10;

const float PRESS1_OFFSET = 0.0;
const float TEMP_OFFSET = 0.16;
const float MIN_PRESS = 2.5;
const float MAX_PRESS = 3.5;

// Variables
volatile int pulseCount = 0;
unsigned long lastTime = 0;

//Measured values
float pressure = 0;
float temperature = 0;
float flow = 0;

bool ledState = false;
bool motor = false;
bool manualOverride = false;
bool manualMotorState = false;
bool mainSwitch = true;
bool lights = true;
bool error = false;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncEventSource events("/events");

// Create an ADS1115 instance
Adafruit_ADS1115 ads;

// Track Wi-Fi status
bool wifiConnected = false;
unsigned long wifiReconnectInterval = 15000;
unsigned long lastWifiReconnectAttempt = 0;

void IRAM_ATTR pulseCounter() {
    pulseCount++;
}

void setup() {
    Serial.begin(115200);
    esp_reset_reason_t reset_reason = esp_reset_reason();
    Serial.print("Reset reason: ");
    Serial.println(reset_reason);
   

    initLittleFS();
    setupWiFi();
    mqttClient.setBufferSize(2048);
    setupMQTT();
    webRoutes();
    server.serveStatic("/", LittleFS, "/");
    server.addHandler(&events);
    ElegantOTA.begin(&server);
    server.begin();
    Serial.println("Web server started");
    
    pinMode(FLOW_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, RISING);
    pinMode(MOTOR_PIN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(ledPin, OUTPUT);
    digitalWrite(MOTOR_PIN, LOW);
    lastTime = millis();
    digitalWrite(LED_RED, HIGH);

    if (!ads.begin()) {
        Serial.println("Failed to initialize ADS1115!");
        while(1);
    }

    Serial.println("ADS1115 initialized!");
    ads.setGain(GAIN_ONE);
    digitalWrite(LED_RED, LOW);
    delay(2000);
}

void loop() {
    ElegantOTA.loop();

    if (!wifiConnected && millis() - lastWifiReconnectAttempt > wifiReconnectInterval) {
        Serial.println("Attempting to reconnect to Wi-Fi...");
        lastWifiReconnectAttempt = millis();
        setupWiFi();
    }

    if (wifiConnected && !mqttClient.connected() && 
        millis() - lastMqttReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
        lastMqttReconnectAttempt = millis();
        if (reconnectMQTT()) {
            lastMqttReconnectAttempt = 0;
        }
    }
    mqttClient.loop();


    if(error){
        mainSwitch = false;
    }
    
    unsigned long currentTime = millis();
    if (currentTime - lastTime >= 1000) { //Measures every second
        noInterrupts();
        flow = (float) pulseCount / 8;
        pulseCount = 0;
        interrupts();
        lastTime = millis();

        pressure = readInPressure();
        pressure = random(1, 10);
        temperature = random(5, 25);
        temperature = readTemperature();

        //Motor control logic
        if (mainSwitch) {
            if (manualOverride) {
                motor = manualMotorState;
            } else {
                if (pressure <= MIN_PRESS) {
                    motor = true;
                } else if (pressure >= MAX_PRESS) {
                    motor = false;
                }
            }
        } else {
            motor = false;
        }

        updateLights(); //Control lights
        digitalWrite(MOTOR_PIN, motor ? HIGH : LOW); //Control motor

        Serial.print("Override: ");
        Serial.println(manualOverride ? "ON" : "OFF");
        Serial.print("Pressure: ");
        Serial.print(pressure);
        Serial.println(" bar");
        Serial.print("Motor status (logic): ");
        Serial.println(motor ? "ON" : "OFF");

        notifyClients(); //update WebPage
        publishState(); //publish MQTT
    }
}

void updateLights() {
    if(lights){
        if (error) {
            // Error state - red LED blinking
            digitalWrite(LED_RED, (millis() % 1000) < 500);
            digitalWrite(LED_GREEN, LOW);
            digitalWrite(LED_BLUE, LOW);
        }
        else if (!mainSwitch) {
            // System off - solid red
            digitalWrite(LED_RED, HIGH);
            digitalWrite(LED_GREEN, LOW);
            digitalWrite(LED_BLUE, LOW);
        } 
        else if (manualOverride) {
            // Manual mode - Yellow
            digitalWrite(LED_RED, !motor ? HIGH : LOW);
            digitalWrite(LED_GREEN, !motor ? HIGH : LOW);
            digitalWrite(LED_BLUE, motor ? HIGH : LOW);
        } 
        else {
            // Auto mode waiting - green
            digitalWrite(LED_RED, LOW);
            digitalWrite(LED_GREEN, !motor ? HIGH : LOW);
            digitalWrite(LED_BLUE, motor ? HIGH : LOW);
        }
    }
    else{
        digitalWrite(LED_RED, LOW);
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_BLUE, LOW);
    }
}

float readInPressure() {
    float voltagePressure = readFromADS1115(0);
    float press = (1.25 * voltagePressure) - 0.625 + PRESS1_OFFSET;
    return press;
}

float readTemperature() {
    float temp_voltage = analogRead(TEMP_PIN) * (3.3 / 4096.0) + TEMP_OFFSET;
    float temp_resistance = 97 * (1/ ((3.3/temp_voltage) - 1));
    float temp = -26.92 * log(temp_resistance) + 0.0796 * temp_resistance + 126.29;
    return temp;
}

float readAverageVoltage(int pin) {
    float total = 0.0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        total += analogRead(pin);
        delay(DELAY);
    }
    return (total / NUM_SAMPLES) * (3.3 / 4095.0);
}

float readFromADS1115(uint8_t channel) {
    int16_t rawValue = ads.readADC_SingleEnded(channel);
    float voltage = rawValue * (4.096 / 32768.0);
    return voltage;
}

void checkForErrors() {
    if(mainSwitch){
        if (pressure < 2.0 || pressure > 4) {
            error = true;
        }
    }
    if(error){
        mainSwitch = false;
    }
}

void setupWiFi() {
    WiFi.mode(WIFI_STA);
    Serial.print("Connecting to ");
    Serial.println(SSID);
    WiFi.begin(SSID, PASSWORD);
    
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("\nConnected to ");
        Serial.println(SSID);
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        wifiConnected = true;
    } else {
        Serial.println("\nFailed to connect to WiFi. Running in offline mode.");
        wifiConnected = false;
    }
}

void initLittleFS() {
    if (!LittleFS.begin(true)) {
        Serial.println("An error has occurred while mounting LittleFS");
    }
    else {
        Serial.println("LittleFS mounted successfully");
    }
}

void notifyClients() {
    DynamicJsonDocument doc(256);
    doc["pressure"] = pressure;
    doc["temperature"] = temperature;
    doc["flow"] = flow;
    doc["motor"] = motor;
    doc["manualOverride"] = manualOverride;
    
    String json;
    serializeJson(doc, json);
    
    Serial.println("Sending: " + json);
    events.send(json.c_str(), "update", millis());
}

void webRoutes(){
    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });

    server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/app.js", "application/javascript");
    });

    server.on("/command", HTTP_POST, [](AsyncWebServerRequest *request) {
    }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        DynamicJsonDocument doc(128);
        DeserializationError error = deserializeJson(doc, data, len);
        
        if (!error) {
            if (doc.containsKey("command")) {
                String command = doc["command"];
                if (command == "toggle") {
                    manualOverride = true;
                    manualMotorState = !manualMotorState;
                } else if (command == "override") {
                    manualOverride = !manualOverride;
                }
                
                notifyClients();
                publishState();
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
        }
        request->send(400, "application/json", "{\"error\":\"invalid request\"}");
    });

    events.onConnect([](AsyncEventSourceClient *client) {
        Serial.println("Client connected to /events");
        notifyClients();
    });
}

//MQTT

void setupMQTT() {
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    sendAutoDiscoveryConfigs();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Convert payload to string
    String message;
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.print("MQTT message arrived [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(message);

    // Define command topics
    String motorTopic = String("homeassistant/") + DEVICE_ID + "/motor/set";
    String overrideTopic = String("homeassistant/") + DEVICE_ID + "/override/set";
    String mainTopic = String("homeassistant/") + DEVICE_ID + "/main/set";
    String errorTopic = String("homeassistant/") + DEVICE_ID + "/error/set";

    // Handle incoming commands
    if (String(topic) == motorTopic) {
        manualMotorState = (message == "ON");
        Serial.print("Manual motor control set to: ");
        Serial.println(manualMotorState ? "ON" : "OFF");
    } 
    else if (String(topic) == overrideTopic) {
        manualOverride = (message == "ON");
        Serial.print("Manual override set to: ");
        Serial.println(manualOverride ? "ON" : "OFF");
    }
    else if (String(topic) == mainTopic) {
        mainSwitch = (message == "ON");
        Serial.print("Main switch set to: ");
        Serial.println(mainSwitch ? "ON" : "OFF");
    }
    else if (String(topic) == errorTopic) {
        error = (message == "ON");
        Serial.print("Error state set to: ");
        Serial.println(error ? "ON" : "OFF");
    }

    publishState();
    notifyClients();
}


bool reconnectMQTT() {
    if (mqttClient.connect(DEVICE_ID, MQTT_USER, MQTT_PASSWORD)) {
        Serial.println("MQTT connected");
        
        // Subscribe to all command topics
        String motorTopic = String("homeassistant/") + DEVICE_ID + "/motor/set";
        String overrideTopic = String("homeassistant/") + DEVICE_ID + "/override/set";
        String mainTopic = String("homeassistant/") + DEVICE_ID + "/main/set";
        String errorTopic = String("homeassistant/") + DEVICE_ID + "/error/set";
        
        mqttClient.subscribe(motorTopic.c_str());
        mqttClient.subscribe(overrideTopic.c_str());
        mqttClient.subscribe(mainTopic.c_str());
        mqttClient.subscribe(errorTopic.c_str());
        
        Serial.println("Subscribed to command topics:");
        Serial.println(motorTopic);
        Serial.println(overrideTopic);
        Serial.println(mainTopic);
        Serial.println(errorTopic);
        
        sendAutoDiscoveryConfigs();
        return true;
    }
    Serial.print("MQTT connection failed, rc=");
    Serial.println(mqttClient.state());
    return false;
}
void sendAutoDiscoveryConfigs() {
    if (!mqttClient.connected()) return;

    Serial.println("Sending auto-discovery configs...");
    
    // Device configuration
    DynamicJsonDocument deviceDoc(256);
    deviceDoc["identifiers"] = DEVICE_ID;
    deviceDoc["name"] = DEVICE_NAME;
    deviceDoc["manufacturer"] = DEVICE_MANUFACTURER;
    deviceDoc["model"] = DEVICE_MODEL;
    deviceDoc["sw_version"] = DEVICE_VERSION;

    // Common state topic
    String stateTopic = String("homeassistant/") + DEVICE_ID + "/state";

    // Pressure sensor
    createSensorConfig(
        "pressure", 
        "Pressure", 
        "bar", 
        "pressure",
        "mdi:gauge",
        deviceDoc,
        stateTopic,
        "pressure"
    );

    // Temperature sensor
    createSensorConfig(
        "temperature", 
        "Temperature", 
        "°C", 
        "temperature",
        "mdi:thermometer",
        deviceDoc,
        stateTopic,
        "temperature"
    );

    // Flow sensor
    createSensorConfig(
        "flow", 
        "Flow Rate", 
        "L/min", 
        nullptr,
        "mdi:water",
        deviceDoc,
        stateTopic,
        "flow"
    );

    // Motor switch
    createSwitchConfig(
        "motor",
        "Pump Motor",
        "mdi:pump",
        deviceDoc,
        stateTopic,
        "motor",
        String("homeassistant/") + DEVICE_ID + "/motor/set"
    );
    //Main Switch
    createSwitchConfig(
        "main",
        "Main Power",
        "mdi:power",
        deviceDoc,
        stateTopic,
        "main",
        String("homeassistant/") + DEVICE_ID + "/main/set"
    );

    // Override switch
    createSwitchConfig(
        "override",
        "Manual Override",
        "mdi:account-wrench",
        deviceDoc,
        stateTopic,
        "override",
        String("homeassistant/") + DEVICE_ID + "/override/set"
    );
    createSwitchConfig(
        "error",
        "System Error",
        "mdi:alert",
        deviceDoc,
        stateTopic,
        "error",
        String("homeassistant/") + DEVICE_ID + "/error/set"
    );
}

void createSensorConfig(const char* sensorId, const char* name, const char* unit, 
                      const char* deviceClass, const char* icon,
                      JsonDocument& deviceDoc, const String& stateTopic,
                      const char* valueKey) {
    String configTopic = String("homeassistant/sensor/") + DEVICE_ID + "_" + sensorId + "/config";
    
    DynamicJsonDocument doc(512);
    doc["name"] = name;
    doc["unique_id"] = String(DEVICE_ID) + "_" + sensorId;
    doc["state_topic"] = stateTopic;
    doc["value_template"] = String("{{ value_json.") + valueKey + " }}";
    if (unit) doc["unit_of_measurement"] = unit;
    if (deviceClass) doc["device_class"] = deviceClass;
    if (icon) doc["icon"] = icon;
    doc["device"] = deviceDoc;
    
    publishDiscovery(configTopic, doc);
}

void createSwitchConfig(const char* switchId, const char* name, const char* icon,
                      JsonDocument& deviceDoc, const String& stateTopic,
                      const char* valueKey, const String& commandTopic) {
    String configTopic = String("homeassistant/switch/") + DEVICE_ID + "_" + switchId + "/config";
    
    DynamicJsonDocument doc(512);
    doc["name"] = name;
    doc["unique_id"] = String(DEVICE_ID) + "_" + switchId;
    doc["state_topic"] = stateTopic;
    doc["command_topic"] = commandTopic;
    doc["value_template"] = String("{{ value_json.") + valueKey + " }}";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    if (icon) doc["icon"] = icon;
    doc["device"] = deviceDoc;
    
    publishDiscovery(configTopic, doc);
}


void sendSwitchConfig(const char* switchId, const char* name, const char* icon, 
                     const char* valueTemplate, JsonDocument& deviceDoc) {
    char topic[100];
    snprintf(topic, sizeof(topic), "homeassistant/switch/%s_%s/config", DEVICE_ID, switchId);

    DynamicJsonDocument doc(512);
    doc["name"] = name;
    doc["icon"] = icon;
    doc["state_topic"] = String("homeassistant/switch/") + DEVICE_ID + "/state";
    doc["command_topic"] = String("homeassistant/switch/") + DEVICE_ID + "/set";
    doc["value_template"] = String("{{ value_json.") + valueTemplate + " }}";
    doc["unique_id"] = String(DEVICE_ID) + "_" + switchId;
    doc["device"] = deviceDoc;

    String config;
    serializeJson(doc, config);
    mqttClient.publish(topic, config.c_str(), true);
}

void publishState() {
    if (!mqttClient.connected()) return;

    DynamicJsonDocument doc(256);
    doc["pressure"] = pressure;
    doc["temperature"] = temperature;
    doc["flow"] = flow;
    doc["motor"] = motor ? "ON" : "OFF";
    doc["override"] = manualOverride ? "ON" : "OFF";
    doc["main"] = mainSwitch ? "ON" : "OFF";  // Add main switch state
    doc["error"] = error ? "ON" : "OFF";  // Add error state
    
    String state;
    serializeJson(doc, state);
    
    String stateTopic = String("homeassistant/") + DEVICE_ID + "/state";
    mqttClient.publish(stateTopic.c_str(), state.c_str(), true);
    
    Serial.print("Published state: ");
    Serial.println(state);
}

void publishDiscovery(const String& topic, JsonDocument& config) {
    String payload;
    serializeJson(config, payload);
    
    Serial.print("Publishing discovery to ");
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(payload);
    
    bool published = mqttClient.publish(topic.c_str(), payload.c_str(), true);
    if (!published) {
        Serial.println("Discovery publish failed!");
    }
}