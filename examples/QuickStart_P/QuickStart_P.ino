#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <Ticker.h>
#include <PangolinMQTT.h>
#include<string>

#define WIFI_SSID "XXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXX"

#define MQTT_HOST IPAddress(192, 168, 1, 21)
#define MQTT_PORT 1883

#define RECONNECT_DELAY_W 5

uint32_t elapsed=0;

PangolinMQTT mqttClient;
Ticker mqttReconnectTimer;
Ticker wifiReconnectTimer;

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

#ifdef ARDUINO_ARCH_ESP8266
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(RECONNECT_DELAY_W, connectToWifi);
}

#else
void WiFiEvent(WiFiEvent_t event) {
    Serial.printf("[WiFi-event] event: %d\n", event);
    switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        connectToMqtt();
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        Serial.println("WiFi lost connection");
        mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        wifiReconnectTimer.once(RECONNECT_DELAY_W, connectToWifi);
        break;
    }
}
#endif

std::string pload0="multi-line payload hex dumper which should split over several lines, with some left over";
std::string pload1="PAYLOAD QOS1";
std::string pload2="Save the Pangolin!";

void onMqttConnect(bool sessionPresent) {
  elapsed=millis();
  Serial.printf("Connected to MQTT session=%d max payload size=%d\n",sessionPresent,mqttClient.getMaxPayloadSize());
  Serial.println("Subscribing at QoS 2");
  mqttClient.subscribe("test", 2);
  Serial.printf("T=%u Publishing at QoS 0\n",millis());
  mqttClient.publish("test", 0, false, pload0);
  Serial.printf("T=%u Publishing at QoS 1\n",millis());
  mqttClient.publish("test", 1, false, pload1);
  Serial.printf("T=%u Publishing at QoS 2\n",millis());
  mqttClient.publish("test", 2, false, pload2);
}

void onMqttDisconnect(int8_t reason) {
  Serial.printf("Disconnected from MQTT reason=%d\n",reason);

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttMessage(const char* topic, uint8_t* payload, struct PANGO_PROPS props, size_t len, size_t index, size_t total) {
  Serial.printf("T=%u Message %s qos%d dup=%d retain=%d len=%d elapsed=%u\n",millis(),topic,props.qos,props.dup,props.retain,len,millis()-elapsed);
  PANGO::dumphex(payload,len,16);
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nPangolinMQTT v%s\n", PANGO_VERSION);

#ifdef ARDUINO_ARCH_ESP8266
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
#else
  WiFi.onEvent(WiFiEvent);
#endif

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCleanSession(true);
  connectToWifi();
}

void loop() {}