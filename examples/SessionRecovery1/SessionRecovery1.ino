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
 */                    
#define WIFI_SSID "XXXXXXXX"
#define WIFI_PASSWORD "XXXXXXXX"
#define MQTT_HOST IPAddress(192, 168, 1, 21)
// if using TLS, edit config.h and #define ASYNC_TCP_SSL_ENABLED 1
// do the same in async_config.h of the PATCHED ESPAsyncTCP library!! 
const uint8_t cert[20] = { 0x9a, 0xf1, 0x39, 0x79,0x95,0x26,0x78,0x61,0xad,0x1d,0xb1,0xa5,0x97,0xba,0x65,0x8c,0x20,0x5a,0x9c,0xfa };
// If using MQTT server authentication, fill in next two fields!
const char* mqAuth="example";
const char* mqPass="pangolin";
//
//  Some sketches will require you to set START_WITH_CLEAN_SESSION to false
//  For THIS sketch, leave it at false
//
#define START_WITH_CLEAN_SESSION   false

#include "framework.h" // manages automatic connection / reconnection and runs setup() - put YOUR stuff in userSetup
//
//  The actual logic of the THIS sketch
//
#include<set>
//  
//  Default/initial values: FIX!!!
#define QOS                 1
#define TRANSMIT_RATE    1000
#define HEARTBEAT           7
// NB RATE IS IN MILLISECONDS!!!!
//
std::string seqTopic="sequence"; // the out-and-back message

uint32_t  sequence=0;
bool      bursting=false; // send clock interlock

std::set<uint32_t> sent;
uint32_t sentThisSession=0;
uint32_t nDups=0;

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
  mqttReconnectTimer.once(RECONNECT_DELAY_M, connectToMqtt);
}

void onMqttMessage(const char* topic, const uint8_t* payload, size_t len,uint8_t qos,bool retain,bool dup) {
//    PANGO::dumphex(payload,len);
    uint32_t R;
    mqttClient.xPayload(payload,len,R);
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
void userSetup(){
    PT1.attach(HEARTBEAT,[]{
        Serial.printf("Heartbeat: Heap=%u seq=%d nDups=%d\n",ESP.getFreeHeap(),sequence,nDups);
        Serial.printf("No. Incomplete send/rcv pairs=%d\n",sent.size());
        if(sent.size()){
            for(auto const& s:sent) Serial.printf("%d,",s);
            Serial.println();
        }
      });
}

void loop(){}