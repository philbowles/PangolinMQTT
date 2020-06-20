/*
 * If you remove the following line, this sketch will compile 
 * using AsyncMqttClient and therefore probably fail - 
 *  the point of these examples, after all is to highlight the 
 *  many bugs in that library.
 *  
 *  Hopefully, when using #define USE_PANGOLIN it should work
 *  as expected :)
 */
#define USE_PANGOLIN
//
//    Common to all sketches: necssary infrastructure
//
#define WIFI_SSID "XXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXX"

#define MQTT_HOST IPAddress(192, 168, 1, 21)
#define MQTT_PORT 1883
//
//  Some sketches will require you to set START_WITH_CLEAN_SESSION to false
//  For THIS sketch, leave it at true
//
#define START_WITH_CLEAN_SESSION   true

#include "options.h"

extern void unifiedSubscribe(std::string,uint8_t);
extern void unifiedPublish(std::string,uint8_t,bool,uint8_t*,size_t);
extern std::string uTopic(std::string t); // automatically prefixes the topic with "A" or "P"
//
//  The actual logic of the THIS sketch
//
#define QOS 2
uint32_t  sequence=0;
std::string topic="sequence";
#define TRANSMIT_RATE 100

void unifiedSetup(){
  T1.attach(1,[]{ Serial.printf("********************** T=%u FH=%u\n",millis(),ESP.getFreeHeap()); });
} // nothing to do here in this sketch  Ticker timers T1,T2 and T3 already defined if you need any

void unifiedMqttConnect() {
  Serial.printf("Subscribe to %s at QOS%d\n",uTopic(topic).c_str(),QOS);
  unifiedSubscribe(uTopic(topic),QOS);

  T2.attach_ms(TRANSMIT_RATE,[]{
      char strSequence[16]; // way more than we need
      sprintf(strSequence,"%d",++sequence);
//      Serial.printf("Publish %s %s @ QoS%d\n",uTopic(topic).c_str(),strSequence,QOS);
      unifiedPublish(uTopic(topic), QOS, false, (uint8_t*) strSequence, strlen(strSequence)+1);
  });
}

void unifiedMqttMessage(std::string topic, uint8_t* payload, uint8_t qos, bool dup, bool retain, size_t len, size_t index, size_t total) {
  static int expected=0;
  int received=payloadToInt(payload,len);
  ++expected;
//  Serial.printf("QoS%d Expecting %d, received %d\n",qos,++expected,received);
  if(received!=expected){
    Serial.printf("\n\nOUT OF ORDER: expecting %d, received %d\n\n",expected,received);
  }
}
