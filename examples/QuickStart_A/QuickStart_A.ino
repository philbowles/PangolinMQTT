/*
        Default is ESP8266 - if you want to compile for ESP32, Look a the code in QuickStart_P
        and splice the relevant bits of code in here
*/
#include <ESP8266WiFi.h>
#include <AsyncMqttClient.h>
#include<string>

#define WIFI_SSID "XXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXX"

#define MQTT_HOST IPAddress(192, 168, 1, 21)
#define MQTT_PORT 1883

uint32_t elapsed=0;

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

std::string pload="multi-line payload hex dumper which should split over several lines, with some left over";

void onMqttConnect(bool sessionPresent) {
  elapsed=millis();
  Serial.printf("Connected to MQTT session=%d max payload FIXED\n",sessionPresent);
  Serial.println("Subscribing at QoS 2");
  mqttClient.subscribe("test", 2);
  Serial.printf("T=%lu Publishing at QoS 0\n",millis());
  mqttClient.publish("test", 0, false, pload.c_str());
  Serial.printf("T=%lu Publishing at QoS 1\n",millis());
  mqttClient.publish("test", 1, false, pload.c_str());
  Serial.printf("T=%lu Publishing at QoS 2\n",millis());
  mqttClient.publish("test", 2, false, pload.c_str());
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.printf("Disconnected from MQTT reason=%d\n",reason);

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties props, size_t len, size_t index, size_t total) {
  Serial.printf("T=%lu Message %s qos%d dup=%d retain=%d len=%d elapsed=%lu\n",millis(),topic,props.qos,props.dup,props.retain,len,millis()-elapsed);
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nAsyncMqttClient v0.8.2\n");

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();
}

void loop() {}