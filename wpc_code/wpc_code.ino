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
#include "mqtt_handler.h"

// Wi-Fi credentials
const char* SSID = "YOUR_WIFI_SSID";
const char* PASSWORD = "YOUR_WIFI_PASSWORD";

// MQTT configuration
const char* MQTT_SERVER = "YOUR_MQTT_IP";
const int MQTT_PORT = 1883;
const char* MQTT_USER = "YOUR_MQTT_USER";
const char* MQTT_PASSWORD = "YOUR_MQTT_PASSWORD";
const char* DEVICE_NAME = "wpc";
const char* DEVICE_ID = "wpc1";
const char* DEVICE_MANUFACTURER = "JorgeS15";
const char* DEVICE_MODEL = "Water Pressure Controller";
const char* DEVICE_VERSION = "2.1.0";

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
const int NUM_SAMPLES = 50;
const int DELAY = 5;

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
bool rebootRequested = false;

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
    setupPins();
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

    if (rebootRequested) {
        delay(1000);
        ESP.restart();
    }

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
    
    unsigned long currentTime = millis();
    if (currentTime - lastTime >= 1000) { //Measures every second
        noInterrupts();
        flow = pulseCount / 8.0;
        pulseCount = 0;
        interrupts();
        lastTime = millis();
        
        pressure = readInPressure();
        temperature = readAverageTemperature();

        controlMotor(); //Controls Motor Logic
        updateLights(); //Control lights

        notifyClients(); //update WebPage
        publishState(); //publish MQTT
        updateSerial(); //Print Serial
    }
    checkForErrors();
}

void controlMotor(){
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
    digitalWrite(MOTOR_PIN, motor ? HIGH : LOW); //Control motor
}

void setupPins(){
    pinMode(FLOW_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(FLOW_PIN), pulseCounter, RISING);
    pinMode(MOTOR_PIN, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(ledPin, OUTPUT);
}

void updateSerial(){
    Serial.print("Override: ");
    Serial.println(manualOverride ? "ON" : "OFF");
    Serial.print("Pressure: ");
    Serial.print(pressure);
    Serial.println(" bar");
    Serial.print("Motor status (logic): ");
    Serial.println(motor ? "ON" : "OFF");
}

void updateLights() {
    if(lights){
        if (error) {
            // Error state - Red blinking
            digitalWrite(LED_RED, (millis() % 1000) < 500);
            digitalWrite(LED_GREEN, LOW);
            digitalWrite(LED_BLUE, LOW);
        }
        else if (!mainSwitch) {
            // System off - Solid red
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
            // Auto mode waiting - Green
            digitalWrite(LED_RED, LOW);
            digitalWrite(LED_GREEN, !motor ? HIGH : LOW);
            digitalWrite(LED_BLUE, motor ? HIGH : LOW);
        }
    }
    else{ //Lights Off
        digitalWrite(LED_RED, LOW);
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_BLUE, LOW);
    }
}

float readInPressure() {
    float voltagePressure = readAveragePressure(0);
    float press = (1.25 * voltagePressure) - 0.625 + PRESS1_OFFSET;
    return press;
}

float readAverageTemperature() {
    float total = 0.0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        total += analogRead(TEMP_PIN);
        delay(DELAY);
    }
    float temp_voltage = total / NUM_SAMPLES * (3.3 / 4096.0) + TEMP_OFFSET;
    float temp_resistance = 97 * (1/ ((3.3/temp_voltage) - 1));
    float temp = -26.92 * log(temp_resistance) + 0.0796 * temp_resistance + 126.29;
    return temp;
}

float readAveragePressure(int pin) {
    float total = 0.0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        total += readFromADS1115(pin);
        delay(DELAY);
    }
    return (total / NUM_SAMPLES);
}

float readFromADS1115(uint8_t channel) {
    int16_t rawValue = ads.readADC_SingleEnded(channel);
    float voltage = rawValue * (4.096 / 32768.0);
    return voltage;
}

void checkForErrors() {
    if(!manualOverride){
        if (pressure < 2.0 || pressure > 4) {
            error = true;
        }
    }
    if(error){
        motor = false;
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
    // Route for root / web page (with authentication)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });

    server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/app.js", "application/javascript");
    });
    //diagnotics page 
    server.on("/diagnostics", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(512);
        doc["uptime"] = millis() / 1000;
        doc["heap"] = ESP.getFreeHeap();
        doc["resetReason"] = esp_reset_reason();
        doc["wifiStrength"] = WiFi.RSSI();
        
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    server.on("/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        rebootRequested = true;
        request->send(200, "application/json", "{\"status\":\"ok\"}");
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