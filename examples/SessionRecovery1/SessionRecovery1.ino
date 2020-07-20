/*
 *  QoS1 guarantees to deliver meesages At least ONCE so if QoS1
 *  works, we will get back every message we sent, 1:1 plus possibly
 *  some duplicates, which we will ignore. (As long as your
 *  broker has persistence true in its .conf file)
 *
 *  Any gaps in the sequence mean that QoS1 is broken.
 *
 *  Warning: if you send messages faster than MQTT can acknowledge them,
 *  they may come back out-of-sequence, but as long as MQTT can keep up
 *  ON AVERAGE then you will still always get them all.
 *
 *
 * PLEASE READ THE NOTES ON THIS SKETCH FIRST AT
 *
 * https://github.com/philbowles/Pangolin/
 *
 * If you remove the following line, this sketch will compile
 * using AsyncMqttClient to allow you to compare results / performance
 */
#define USE_PANGOLIN
#define USE_TLS

#include<set>
//
//    Common to all sketches: necssary infrastructure
//
#define WIFI_SSID "XXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXX"

#define MQTT_HOST IPAddress(192, 168, 1, 21)

#ifdef USE_TLS
#define MQTT_PORT 8883
const uint8_t cert[20] = { 0x9a, 0xf1, 0x39, 0x79,0x95,0x26,0x78,0x61,0xad,0x1d,0xb1,0xa5,0x97,0xba,0x65,0x8c,0x20,0x5a,0x9c,0xfa };
#else
#define MQTT_PORT 1883
#endif
//
//  Some sketches will require you to set START_WITH_CLEAN_SESSION to false
//  For THIS sketch, leave it at false
//
#define START_WITH_CLEAN_SESSION   false

#include "options.h"
//
// unified functions that "smooth out" the minor API differences between the libs
//
extern void unifiedPublish(std::string,uint8_t,bool,uint8_t*,size_t);
extern void unifiedSubscribe(std::string,uint8_t);
extern void unifiedUnubscribe(std::string);
// function to automatically add A or P prefix to a topic
extern std::string uTopic(std::string t); // automatically prefixes the topic with "A" or "P"
//
//  The actual logic of the THIS sketch
//
//
//  Default/initial values: FIX!!!
#define QOS                 1
#define TRANSMIT_RATE    1000
#define HEARTBEAT           7
// NB RATE IS IN MILLISECONDS!!!!
//
std::string seqTopic=uTopic("sequence"); // the out-and-back message

uint32_t  sequence=0;
bool      bursting=false; // send clock interlock

std::set<uint32_t> sent;
uint32_t sentThisSession=0;
uint32_t nDups=0;

void sendNextInSequence(){
  char buf[16];
  sprintf(buf,"%d",++sequence);
  sent.insert(sequence);
  ++sentThisSession;
  unifiedPublish(seqTopic.c_str(), QOS, false, (uint8_t*) buf, strlen(buf)+1);
  if(random(0,100) > 95) {
    Serial.printf("DELIBERATE RANDOM DISCONNECT TO TEST QOS!!!\n");
    mqttClient.disconnect();
  }
  Serial.printf("SENT %s (thisSession=%d)\n",buf,sentThisSession);
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

void unifiedMqttConnect() {
  unifiedSubscribe(seqTopic.c_str(),QOS); // T-O-F topic @ chosen QoS
  startClock();
}

void unifiedMqttDisconnect(int8_t reason) {
  Serial.printf("USER: Disconnect reason=%d\n",reason);
  sentThisSession=0;
  stopClock();
}

void unifiedMqttMessage(std::string topic, uint8_t* payload, uint8_t qos, bool dup, bool retain, size_t len, size_t index, size_t total) {
//    Serial.printf("unifiedMqttMessage %s len=%d\n",topic.c_str(),len);
//    PANGO::dumphex(payload,len);
    uint32_t R=PANGO::payloadToInt(payload,len);
    if(!sentThisSession){
      Serial.printf("***** QoS1 recovery in action! *****\n");
      Serial.printf("We have not yet sent any messages, but %d just came in!\n",R);
    }
    Serial.printf("RCVD %u\n",R);
    if(dup) {
      Serial.printf("QOS1 DUPLICATE VALUE RECEIVED / IGNORED %d\n",R);
      ++nDups;
    }
    sent.erase(R);
}
// set qos topic names and start HB ticker
void unifiedSetup(){
    PT1.attach(HEARTBEAT,[]{
        Serial.printf("Heartbeat: Heap=%u seq=%d nDups=%d number of reconnects=%d\n",ESP.getFreeHeap(),sequence,nDups,nRCX);
        Serial.printf("No. Incomplete send/rcv pairs=%d\n",sent.size());
        if(sent.size()){
            for(auto const& s:sent) Serial.printf("%d,",s);
            Serial.println();
        }
      });
}