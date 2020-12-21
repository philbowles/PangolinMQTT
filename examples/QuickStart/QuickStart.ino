//
//    Common to all sketches: necssary infrastructure identifiers
//
#define WIFI_SSID "XXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXX"
#define MQTT_HOST IPAddress(192, 168, 1, 4)
// if using TLS, edit config.h and #define ASYNC_TCP_SSL_ENABLED 1
// do the same in async_config.h of the PATCHED ESPAsyncTCP library!! 
const uint8_t cert[20] = { 0x9a, 0xf1, 0x39, 0x79,0x95,0x26,0x78,0x61,0xad,0x1d,0xb1,0xa5,0x97,0xba,0x65,0x8c,0x20,0x5a,0x9c,0xfa };
// If using MQTT server authentication, fill in next two fields!
const char* mqAuth="";
const char* mqPass="";
//
//  Some sketches will require you to set START_WITH_CLEAN_SESSION to false
//  For THIS sketch, leave it at false
//
#define START_WITH_CLEAN_SESSION   true

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