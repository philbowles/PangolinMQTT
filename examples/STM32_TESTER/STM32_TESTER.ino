#include <LwIP.h>
#include <STM32Ethernet.h>
#include <PangolinMQTT.h>

PangolinMQTT mqttClient;

#define MQTT_HOST IPAddress(192, 168, 1, 4)
#define MQTT_PORT 1883

#define START_WITH_CLEAN_SESSION   true

void onMqttConnect(bool sessionPresent) {
  Serial.printf("Connected as %s: Max safe payload %u\n",mqttClient.getClientId(),mqttClient.getMaxPayloadSize());
  mqttClient.subscribe("stm32",0);
  mqttClient.publish("stm32",0,false,std::string("Oh my god it works"));
}

void onMqttMessage(const char* topic, uint8_t* payload, struct PANGO_PROPS props, size_t len, size_t index, size_t total) {
  PANGO::dumphex(payload,len);
}

void onMqttDisconnect(int8_t reason) {
  Serial.printf("Disconnected from MQTT reason=%d\n",reason);
}
void setup() {
  Serial.begin(115200);
  Ethernet.begin();
//  Serial.print("IP: ");
//  Serial.println(Ethernet.localIP());

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setWill("NUCLEO_DIED",2,false,"It's 'Alpha': probably still some bugs");
  mqttClient.setCleanSession(START_WITH_CLEAN_SESSION);
//  mqttClient.setClientId("f429zi");
  mqttClient.connect();
}


void loop() {

}
