#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

// MQTT configuration
extern const char* MQTT_SERVER;
extern const int MQTT_PORT;
extern const char* MQTT_USER;
extern const char* MQTT_PASSWORD;
extern const char* DEVICE_NAME;
extern const char* DEVICE_ID;
extern const char* DEVICE_MANUFACTURER;
extern const char* DEVICE_MODEL;
extern const char* DEVICE_VERSION;

// Extern declarations for variables needed by MQTT functions
extern float pressure;
extern float temperature;
extern float flow;
extern bool motor;
extern bool manualOverride;
extern bool manualMotorState;
extern bool mainSwitch;
extern bool error;
extern bool rebootRequested;

// Function declarations
void setupMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
bool reconnectMQTT();
void publishState();
void sendAutoDiscoveryConfigs();
void publishDiscovery(const String& topic, JsonDocument& config);
void createSensorConfig(const char* sensorId, const char* name, const char* unit, 
                      const char* deviceClass, const char* icon,
                      JsonDocument& deviceDoc, const String& stateTopic,
                      const char* valueKey);
void createSwitchConfig(const char* switchId, const char* name, const char* icon,
                      JsonDocument& deviceDoc, const String& stateTopic,
                      const char* valueKey, const String& commandTopic);
void createButtonConfig(const char* buttonId, const char* name, const char* icon,
                      JsonDocument& deviceDoc, const String& commandTopic);

#endif