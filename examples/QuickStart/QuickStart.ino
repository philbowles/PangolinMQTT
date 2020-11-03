#define WIFI_SSID "XXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXX"

#define MQTT_HOST IPAddress(192, 168, 1, 4)
#define MQTT_PORT 1883

#define START_WITH_CLEAN_SESSION true

#include "framework.h" // manages automatic connection / reconnection and runs setup() - put YOUR stuff in userSetup

const char* pload0="multi-line payload hex dumper which should split over several lines, with some left over";
const char* pload1="PAYLOAD QOS1";
const char* pload2="Save the Pangolin!";

void onMqttConnect(bool sessionPresent) {
  Serial.printf("Connected to MQTT session=%d max payload size=%d\n",sessionPresent,mqttClient.getMaxPayloadSize());
  Serial.println("Subscribing at QoS 2");
  mqttClient.subscribe("test", 2);
  Serial.printf("T=%u Publishing at QoS 0\n",millis());
  mqttClient.publish("test",pload0,strlen(pload0));
  Serial.printf("T=%u Publishing at QoS 1\n",millis());
  mqttClient.publish("test",pload1,strlen(pload1),1); 
  Serial.printf("T=%u Publishing at QoS 2\n",millis());
  mqttClient.publish("test",pload2,strlen(pload2),2);

  PT1.attach(10,[]{
    // simple way to publish int types  as strings using printf format
    mqttClient.publish("test",ESP.getFreeHeap(),"%u"); 
    mqttClient.publish("test",-33); 
  });
}

void onMqttMessage(const char* topic, const uint8_t* payload, size_t len,uint8_t qos,bool retain,bool dup) {
  Serial.printf("\nH=%u Message %s qos%d dup=%d retain=%d len=%d\n",ESP.getFreeHeap(),topic,qos,dup,retain,len);
  PANGO::dumphex(payload,len,16);
}

void userSetup() {
  // nothing to do
}

void loop() {}