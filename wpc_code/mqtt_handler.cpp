#include "mqtt_handler.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>

// MQTT client object
extern PubSubClient mqttClient;

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
	String rebootTopic = String("homeassistant/") + DEVICE_ID + "/reboot/set";

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
	else if (String(topic) == rebootTopic && message == "PRESS") 
	{
		Serial.println("Reboot requested via MQTT");
		rebootRequested = true;
	}

    publishState();
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

void publishState() {
    if (!mqttClient.connected()) return;

    DynamicJsonDocument doc(256);
    doc["pressure"] = pressure;
    doc["temperature"] = temperature;
    doc["flow"] = flow;
    doc["motor"] = motor ? "ON" : "OFF";
    doc["override"] = manualOverride ? "ON" : "OFF";
    doc["main"] = mainSwitch ? "ON" : "OFF";
    doc["error"] = error ? "ON" : "OFF";
	doc["reboot_requested"] = rebootRequested;
    
    String state;
    serializeJson(doc, state);
    
    String stateTopic = String("homeassistant/") + DEVICE_ID + "/state";
    mqttClient.publish(stateTopic.c_str(), state.c_str(), true);
    
    Serial.print("Published state: ");
    Serial.println(state);
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
    
    // Main Switch
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
	
	createButtonConfig(
		"reboot",
		"Reboot Device",
		"mdi:restart",
		deviceDoc,
		String("homeassistant/") + DEVICE_ID + "/reboot/set"
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

void createButtonConfig(const char* buttonId, const char* name, const char* icon,
                      JsonDocument& deviceDoc, const String& commandTopic) {
    String configTopic = String("homeassistant/button/") + DEVICE_ID + "_" + buttonId + "/config";
    
    DynamicJsonDocument doc(512);
    doc["name"] = name;
    doc["unique_id"] = String(DEVICE_ID) + "_" + buttonId;
    doc["command_topic"] = commandTopic;
    doc["payload_press"] = "PRESS";
    if (icon) doc["icon"] = icon;
    doc["device"] = deviceDoc;
    
    publishDiscovery(configTopic, doc);
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