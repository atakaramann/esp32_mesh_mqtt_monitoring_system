#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <painlessMesh.h>
#include <ArduinoJson.h>

#define ONE_WIRE_BUS  4
#define SENSOR_COUNT 5

#define MESH_PREFIX   "MonitoringSystem"
#define MESH_PASSWORD "meshpass2003"
#define MESH_PORT     5555
#define MESH_CHANNEL  9

OneWire           oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Scheduler     userScheduler;
painlessMesh  mesh;

unsigned long lastReadMs = 0;
const unsigned long READ_INTERVAL = 5000;

void receivedCallback(uint32_t from, String &msg) {
    Serial.printf("[Node1] Received from %u: %s\n", from, msg.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("[Node1] New connection: %u\n", nodeId);
}

void changedConnectionsCallback() {
    Serial.println("[Node1] Topology changed.");
}

void readAndBroadcast() {
    sensors.requestTemperatures();

    JsonDocument doc;
    doc["node"] = 1;
    doc["type"] = "temperature";
    doc["uptimeSec"] = millis() / 1000;

    JsonArray temps = doc["values"].to<JsonArray>();
    int validCount = 0;
    for (int i = 0; i < SENSOR_COUNT; i++) {
        float t = sensors.getTempCByIndex(i);
        if (t == DEVICE_DISCONNECTED_C){
            Serial.printf("[Node1] Sensor %d disconnected!\n", i);
            temps.add(nullptr);
        }
        else{
            temps.add(serialized(String(t, 2)));
            validCount++;
        }
    }

    doc["validCount"] = validCount;
    String msg;
    serializeJson(doc, msg);
    mesh.sendBroadcast(msg);
    Serial.printf("[Node1] Broadcast sent (%d sensors): %s\n", validCount, msg.c_str());
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("[Node1] Starting...");

    sensors.begin();
    int deviceCount = sensors.getDeviceCount();
    Serial.printf("[Node1] DS18B20 device count: %d\n", deviceCount);

    if (deviceCount < SENSOR_COUNT){
        Serial.printf("[Node1] WARNING: Expected %d sensors, found %d!\n", SENSOR_COUNT, deviceCount);
    }
    else{
        Serial.println("[Node1] All DS18B20 sensors detected.");
    }

    mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
    mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, MESH_CHANNEL);
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionsCallback);

    Serial.println("[Node1] Mesh initialized.");
}

void loop() {
    mesh.update();

    if (millis() - lastReadMs >= READ_INTERVAL){
        lastReadMs = millis();
        readAndBroadcast();
    }
}