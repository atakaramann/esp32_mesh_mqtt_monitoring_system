#include <Arduino.h>
#include <DHT.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>

#define DHT_PIN  15
#define DHT_TYPE DHT21

#define MESH_PREFIX   "MonitoringSystem"
#define MESH_PASSWORD "meshpass2003"
#define MESH_PORT     5555
#define MESH_CHANNEL  9

DHT dht(DHT_PIN, DHT_TYPE);

Scheduler    userScheduler;
painlessMesh mesh;

unsigned long lastReadMs = 0;
const unsigned long READ_INTERVAL = 30000;

void receivedCallback(uint32_t from, String &msg) {
    Serial.printf("[Node2] Received from %u: %s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("[Node2] New connection: %u\n", nodeId);
}

void changedConnectionsCallback() {
    Serial.println("[Node2] Topology changed.");
}

void readAndBroadcast() {
    float humidity    = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)){
        Serial.println("[Node2] Read error!");
        return;
    }

    JsonDocument doc;
    doc["node"]        = 2;
    doc["type"]        = "humidity_temp";
    doc["temperature"] = serialized(String(temperature, 2));
    doc["humidity"]    = serialized(String(humidity, 1));
    doc["uptimeSec"]   = millis() / 1000;

    String msg;
    serializeJson(doc, msg);

    mesh.sendBroadcast(msg);
    Serial.printf("[Node2] Broadcast sent: %s\n", msg.c_str());
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("[Node2] Starting...");

    dht.begin();
    delay(2000);
    Serial.println("[Node2] DHT21 ready.");

    mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionsCallback);

    Serial.println("[Node2] Mesh initialized.");
}

void loop() {
    mesh.update();

    if (millis() - lastReadMs >= READ_INTERVAL){
        lastReadMs = millis();
        readAndBroadcast();
    }
}