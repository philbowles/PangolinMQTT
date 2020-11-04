/*                                 
 *  QoS2 guarantees to deliver meesages EXACTLY ONCE so if QoS2      
 *  works, we will get back every message we sent, 1:1 any gaps in the
 *  gaps in the sequence mean that QoS2 is broken.
 *  
 *  Warning: if you send messages fatser than MQTT can acknowledge them,
 *  they may come back out-of-sequence, but as long as MQTT can keep up 
 *  ON AVERAGE then you will still always get them all. 
 */

//#define USE_TLS

#include<set>
//
//    Common to all sketches: necssary infrastructure
//
#define WIFI_SSID "XXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXX"

#define MQTT_HOST IPAddress(192, 168, 1, 4)

#ifdef USE_TLS
#define MQTT_PORT 8883
const uint8_t cert[20] = { 0x9a, 0xf1, 0x39, 0x79,0x95,0x26,0x78,0x61,0xad,0x1d,0xb1,0xa5,0x97,0xba,0x65,0x8c,0x20,0x5a,0x9c,0xfa };
#else
#define MQTT_PORT 1883
#endif
#define MQTT_PORT 1883
//
//  Some sketches will require you to set START_WITH_CLEAN_SESSION to false
//  For THIS sketch, leave it at false
//
#define START_WITH_CLEAN_SESSION   false

#include "framework.h"
//
//  The actual logic of the THIS sketch
//
//  
//  Default/initial values: FIX!!!
#define QOS                 2
#define TRANSMIT_RATE    1000
#define HEARTBEAT           7
// NB RATE IS IN MILLISECONDS!!!!
//
std::string seqTopic="sequence"; // the out-and-back topic

uint32_t  sequence=0;
bool      bursting=false; // send clock interlock

std::set<uint32_t> sent;
uint32_t sentThisSession=0;

void sendNextInSequence(){

  sent.insert(++sequence);
  ++sentThisSession;
  mqttClient.xPublish(seqTopic.c_str(),sequence,QOS, false);
  if(random(0,100) > 95) {
    Serial.printf("DELIBERATE RANDOM DISCONNECT TO TEST QOS!!!\n");
    mqttClient.disconnect(); 
  }
  Serial.printf("SENT %d (thisSession=%d)\n",sequence,sentThisSession);
}

void startClock(){
  if(!bursting) PT2.attach_ms(TRANSMIT_RATE,sendNextInSequence);
  else Serial.printf("Clock Already running!\n");
  bursting=true;
}
void stopClock(){
  if(bursting) PT2.detach();
  else Serial.printf("Clock Already stopped!\n");
  bursting=false;
}

void onMqttConnect(bool sessionPresent) {
  Serial.printf("Connected to MQTT session=%d max payload size=%d\n",sessionPresent,mqttClient.getMaxPayloadSize());
  mqttClient.subscribe(seqTopic.c_str(),QOS); // T-O-F topic @ chosen QoS   
  startClock();
}

void onMqttDisconnect(int8_t reason) {
  Serial.printf("Disconnected from MQTT reason=%d\n",reason);
  sentThisSession=0;
  stopClock();
  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(RECONNECT_DELAY_M, connectToMqtt);
  }
}

void onMqttMessage(const char* topic, const uint8_t* payload, size_t len,uint8_t qos,bool retain,bool dup) {
//    PANGO::dumphex(payload,len);
    uint32_t R;
    mqttClient.xPayload(payload,len,R);
    if(!sentThisSession){
      Serial.printf("***** QoS2 recovery in action! *****\n");
      Serial.printf("We have not yet sent any messages, but %d just came in!\n",R);
    }
    Serial.printf("RCVD %u\n",R);
    if(dup) {
        Serial.printf("QOS2 FAIL!!! DUPLICATE VALUE RECEIVED %d\n",R);
        stopClock();
        PT1.detach();
    }
    sent.erase(R);
}
// set qos topic names and start HB ticker
void userSetup(){
    PT1.attach(HEARTBEAT,[]{
        Serial.printf("Heartbeat: Heap=%u seq=%d\n",ESP.getFreeHeap(),sequence);
        Serial.printf("No. Incomplete send/rcv pairs=%d\n",sent.size());
        if(sent.size()){
            for(auto const& s:sent) Serial.printf("%d,",s);
            Serial.println();
        }
      });
}

void loop(){}