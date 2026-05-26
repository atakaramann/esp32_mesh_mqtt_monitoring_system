#include <Arduino.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define MESH_PREFIX   "MonitoringSystem"
#define MESH_PASSWORD "meshpass2003"
#define MESH_PORT     5555
#define MESH_CHANNEL  9

#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASS     "YOUR_WIFI_PASSWORD"

#define MQTT_SERVER   "YOUR_MQTT_BROKER"
#define MQTT_PORT     1883
#define MQTT_TOPIC    "monitoring/sensors"
#define CLIENT_ID     "ESP32_Gateway_001"

#define SOIL_ANALOG_PIN 1

Scheduler     userScheduler;
painlessMesh  mesh;

WiFiClient    wifiClient;
PubSubClient  mqttClient(wifiClient);

const int SOIL_DRY_VALUE = 4095;
const int SOIL_WET_VALUE = 1500;

unsigned long lastSoilReadMs = 0;
unsigned long lastMqttReconnectMs = 0;

const unsigned long SOIL_INTERVAL = 30000;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;

void connectMQTT() {
  if (mqttClient.connected()) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Gateway] WiFi not connected yet. MQTT connection skipped.");
    return;
  }

  Serial.printf("[Gateway] WiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());

  Serial.println("[Gateway] Connecting to MQTT broker...");
  if (mqttClient.connect(CLIENT_ID)) {
    Serial.println("[Gateway] MQTT connected.");
  } else {
    Serial.printf("[Gateway] MQTT failed, state: %d\n", mqttClient.state());
  }
}

bool publishMQTT(const String &payload) {
  if (!mqttClient.connected()) {
    Serial.println("[Gateway] MQTT not connected. Publish skipped.");
    return false;
  }

  bool ok = mqttClient.publish(MQTT_TOPIC, payload.c_str());

  if (ok) {
    Serial.printf("[Gateway] MQTT publish OK: %s\n", payload.c_str());
  } else {
    Serial.println("[Gateway] MQTT publish FAILED.");
  }

  return ok;
}

float calculateSoilPercent(int raw) {
  float percent = (float)(SOIL_DRY_VALUE - raw) * 100.0 /
                  (float)(SOIL_DRY_VALUE - SOIL_WET_VALUE);

  if(percent < 0)   percent = 0;
  if(percent > 100) percent = 100;
  return percent;
}

void readAndPublishSoil() {
  int raw = analogRead(SOIL_ANALOG_PIN);
  float moisture = calculateSoilPercent(raw);
  Serial.printf("[Gateway] Soil read: raw=%d moisture=%.1f%%\n", raw, moisture);

  JsonDocument doc;
  doc["node"]      = 3;
  doc["type"]      = "soil_moisture";
  doc["raw"]       = raw;
  doc["moisture"]  = serialized(String(moisture, 1));
  doc["uptimeSec"] = millis() / 1000;

  String msg;
  serializeJson(doc, msg);
  
  bool sent = publishMQTT(msg);
  if(!sent){
  Serial.println("[Gateway] Soil message could not be published.");
  }

  mesh.sendBroadcast(msg);
}

void receivedCallback(uint32_t from, String &msg) {
  Serial.printf("[Gateway] Received from node %u: %s\n", from, msg.c_str());

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, msg);

  if (error) {
    Serial.printf("[Gateway] JSON parse error: %s\n", error.c_str());
    return;
  }

  bool sent = publishMQTT(msg);

  if (!sent) {
    Serial.println("[Gateway] Mesh message could not be published.");
  }
}

void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("[Gateway] New connection, nodeId=%u\n", nodeId);
}

void changedConnectionsCallback() {
  Serial.println("[Gateway] Topology changed.");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("[Gateway] Starting...");

  analogReadResolution(12);
  pinMode(SOIL_ANALOG_PIN, INPUT);

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);

  mesh.stationManual(WIFI_SSID, WIFI_PASS);
  mesh.setHostname("ESP32S3-Gateway");

  mesh.setRoot(true);
  mesh.setContainsRoot(true);

  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionsCallback);

  Serial.println("[Gateway] Ready.");
}

void loop() {
  mesh.update();
  mqttClient.loop();

  if (!mqttClient.connected()) {
    if (millis() - lastMqttReconnectMs >= MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnectMs = millis();
      connectMQTT();
    }
  }

  if (millis() - lastSoilReadMs >= SOIL_INTERVAL) {
    lastSoilReadMs = millis();
    readAndPublishSoil();
  }

}
