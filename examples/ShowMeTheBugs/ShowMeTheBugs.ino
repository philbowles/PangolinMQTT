/*
 * PLEASE READ THE NOTES ON THIS SKETCH FIRST AT
 * 
 * https://github.com/philbowles/Pangolin/
 * 
 * If you remove the following line, this sketch will compile 
 * using AsyncMqttClient to allow you to compare results / performance
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
//  Every TRANSMIT_RATE milliseconds, send out block of BURST_SIZE number of messages as QoSX each with
//  a payload of PAYLOAD_SIZE bytes. 
//
//  Save T0 immediately prior to sending the burst of messages
//  Once all BURST_SIZE messages have returned, take T1
//
//  T1-T0 is "flight time" of all BURST_SIZE messages
//  publish simple average per message of T1-T0 / BURST_SIZE to topic [A|P]avg<qos>
//
//  Example with default values using Pangolin: given imaginary T0 and T1 of 4000 and 4500 millis()
//
//  Every 5 seconds send 10 QoS0 messages with payload size of 100bytes
//  publish("Pavg0",50...   = (4500 - 4000) / 10
//  
//  Default/initial values: FIX!!!
#define QOS                  2
#define PAYLOAD_SIZE       100
#define BURST_SIZE           1
#define TRANSMIT_RATE    10000
#define HEARTBEAT           10
/*
#define QOS                  0
#define PAYLOAD_SIZE       100
#define BURST_SIZE           4
#define TRANSMIT_RATE     5000
#define HEARTBEAT           10
*/
// NB RATE IS IN MILLISECONDS!!!!
//
std::string tofTopic=uTopic("tof"); // the out-and-back message
std::string avgTopic=uTopic("avg"); // the simple average of the T-O-F at QosX

uint32_t  startTime; // start of burst in millis()
uint32_t  nInFlight; // how many still not yet received? avg published when this => 0

bool      bursting=false; // TX timer interlock      

uint8_t   ctrlQos=QOS; // can be changed in-flight by publishing "qos" with payload 0 1 or 2
uint32_t  ctrlRate=TRANSMIT_RATE; // can be changed in-flight by publishing "rate" with payload of new TX rate
uint32_t  ctrlSize=PAYLOAD_SIZE; // can be changed in-flight by publishing "size" with payload of new T-O-F size
uint32_t  ctrlBurst=BURST_SIZE; // can be changed in-flight by publishing "size" with payload of new T-O-F size

//
// publish these from any client to change their values for the next round of messages
//
// the QoS to 0, 1 or 2
std::string ctrlTopicQ=("qos");
//
// the rate at which bursts are sent in milliseconds
std::string ctrlTopicR=("rate");
//
// the size of the payload in each timing message sent
std::string ctrlTopicS=("size");
//
// the number of messages in each burts
std::string ctrlTopicB=("burst");
// 
// show current settings
std::string ctrlTopicV=("show");
//
// publish this from any client to pause sending
std::string ctrlTopicX=("stop");
//
// publish this from any client to start sending
std::string ctrlTopicY=("start");


void setTofQosTopic(uint8_t qos){
    tofTopic.pop_back(); // trim qos from topic
    avgTopic.pop_back(); // trim qos from topic
    tofTopic.push_back(qos + 0x30); // add back on new Qos 
    avgTopic.push_back(qos + 0x30); // add back on new Qos 
}

void showValues(){
  Serial.printf("Using QoS%d, payload size %d, burst size %d, rate every %d.%03d second(s)\n",ctrlQos,ctrlSize,ctrlBurst,ctrlRate/1000,ctrlRate%1000);    
  Serial.printf("Round-trip topic=%s, avg topic=%s\n",tofTopic.c_str(),avgTopic.c_str());    
}

void sendBurst(){
  char* buf=static_cast<char*>(malloc(ctrlSize)); // allocate the payload buffer
  for(int i=0;i<ctrlSize;i++) buf[i]='X'; // fill it with known values
  nInFlight=ctrlBurst;
  startTime=millis();
  for(int b=0;b<ctrlBurst;b++) unifiedPublish(tofTopic.c_str(), ctrlQos, false, (uint8_t*) buf, ctrlSize);
  free(buf);
  Serial.printf("I counted them out...(%d)\n",ctrlBurst);
}

void addQosToTopic(uint8_t qos){
   tofTopic.push_back(qos + 0x30); // add on initial Qos e.g. Ptof and qos=1 becomes Ptof1
   avgTopic.push_back(qos + 0x30); // add on initial Qos e.g. Atof and qos=0 becomes Atof0   
}

void changeValues(uint8_t newQos,uint32_t newRate,uint32_t newSize,uint32_t newBurst){
  Serial.printf("CHANGE Q=%d R=%d S=%d B=%d\n",newQos,newRate,newSize,newBurst);
  if(newQos!=ctrlQos){
    Serial.printf("Changing from QoS%d to QoS%d\n",ctrlQos,newQos);
    Serial.printf("Unsubscribe from %s\n",tofTopic.c_str());
    unifiedUnsubscribe(tofTopic.c_str()); // lose old sub
    tofTopic.pop_back(); // trim qos from topic
    avgTopic.pop_back(); // trim qos from topic
    addQosToTopic(newQos);
    ctrlQos=newQos;
    Serial.printf("Subscribe to %s @ QoS%d\n",tofTopic.c_str(),ctrlQos);
    unifiedSubscribe(tofTopic.c_str(),ctrlQos); // T-O-F topic @ chosen QoS   
  }
  if(newRate!=ctrlRate){
    stopClock();
    Serial.printf("Changing from rate %dmS to %dmS\n",ctrlRate,newRate);
    ctrlRate=newRate;
    startClock();
  }
  if(newSize!=ctrlSize){
    Serial.printf("Changing from payload size of %dB to %dB\n",ctrlSize,newSize);
    ctrlSize=newSize;
  }
  if(newBurst!=ctrlBurst){
    Serial.printf("Changing from burst size of %d to %d\n",ctrlBurst,newBurst);
    ctrlBurst=newBurst;
  }
  showValues();
}

void startClock(){
  if(!bursting) T2.attach_ms(ctrlRate,sendBurst);
  else Serial.printf("Clock Already running!\n");
  bursting=true;
}
void stopClock(){
  if(bursting) T2.detach();
  else Serial.printf("Clock Already stopped!\n");
  bursting=false;
}

void unifiedMqttConnect() {
  unifiedSubscribe(ctrlTopicB.c_str(),0); // burst
  unifiedSubscribe(ctrlTopicQ.c_str(),0); // qos
  unifiedSubscribe(ctrlTopicR.c_str(),0); // rate
  unifiedSubscribe(ctrlTopicS.c_str(),0); // size
  unifiedSubscribe(ctrlTopicV.c_str(),0); // show
  unifiedSubscribe(ctrlTopicX.c_str(),0); // stop
  unifiedSubscribe(ctrlTopicY.c_str(),0); // start
  
  unifiedSubscribe(tofTopic.c_str(),ctrlQos); // T-O-F topic @ chosen QoS   
  
  showValues();
  //startClock();
}

void unifiedMqttDisconnect(int8_t reason) {
  Serial.printf("USER: Disconnect reason=%d\n",reason);
  stopClock();
}

void unifiedMqttMessage(std::string topic, uint8_t* payload, uint8_t qos, bool dup, bool retain, size_t len, size_t index, size_t total) {
  Serial.printf("UMM: %s %08X, Q=%d D=%d R=%d l=%d i=%d t=%t\n",topic.c_str(),payload,qos,dup,retain,len,index,total);
  PANGO::dumphex(payload,len);
  if(topic=="qos"){
    uint8_t newqos=payloadToInt(payload,len); 
    if(newqos < 3) changeValues(newqos,ctrlRate,ctrlSize,ctrlBurst);
    else Serial.printf("ERROR! Cannot change to QoS%d!!!\n",newqos);
  }
  else if(topic=="rate") {
    uint32_t newrate=payloadToInt(payload,len); 
    if(newrate > 125 && newrate < 10000) changeValues(ctrlQos,newrate,ctrlSize,ctrlBurst); // arbitrary "sensible" values
    else Serial.printf("ERROR! Rate shuld be between 125 and 10000 (0.125s and 10s) %d is not!!!\n",newrate);
  }
  else if(topic=="size") {
    uint32_t newsize=payloadToInt(payload,len); 
    if(newsize > 16) changeValues(ctrlQos, ctrlRate,newsize,ctrlBurst);  // at least a string value of a massive integer + some safety
    else Serial.printf("ERROR! Payload size %d is too small!!!\n",newsize);
  }
  else if(topic=="burst") {
    uint32_t newburst=payloadToInt(payload,len); 
    if(newburst > 0 && newburst < 20) changeValues(ctrlQos, ctrlRate,ctrlSize,newburst);  // limited by mosquitto in-flight Q size
    else Serial.printf("ERROR! Burst size %d is too small!!!\n",newburst);
  }
  else if(topic=="stop") {
    Serial.printf("Sending Stopped\n");
    stopClock();
  }
  else if(topic=="start") {
    Serial.printf("Sending Started\n");
    startClock();
  }
  else if(topic=="show") { showValues(); }
  else { // default tof topic
    // warn of fragment failure on A
    if(len!=ctrlSize){
      Serial.printf("FRAGMENT FAILURE! Only %d bytes received out of %d\n",len,ctrlSize);
      Serial.printf("Cannot continue without adding packet reassembly code :(\n",len,ctrlSize);
      dumphex(payload,len);
      stopClock();
      return;
    }
    /*
    // verify payload integrity (Count no. of Xs)
    char buf[16];
    strcpy(buf,(char*) payload);
    size_t numXs=len - (strlen(buf)+1);
    //Serial.printf("INTEGRITY: PL=%d raw tof is %s, lenTof is %d: should be %d 'X's\n",len,buf,strlen(buf),numXs);
    uint8_t* pstart=payload+strlen(buf)+1;
    for(uint8_t* c=pstart;c<pstart + numXs;c++){
      if(*c!='X'){
        Serial.printf("Corrupted payload! Found '%c' (0x%02X) at offset %d\n",*c,*c,c - pstart);
        break;
      }
    }
    */
    //
    if(dup){
      Serial.printf("I DON'T COUNT DUPS!\n");
      return;
    }
    if(!(--nInFlight)){ // last one just came home
      uint32_t endTime=millis();
      char avg[16];
      sprintf(avg,"%d",(endTime - startTime) / ctrlBurst);
//      Serial.printf("Start=%u End=%u N=%u Average = %s\n",startTime,endTime,ctrlBurst,avg);
      // always publish avg value at qos0
      unifiedPublish(avgTopic.c_str(), 0, false, (uint8_t*) avg, strlen(avg));
      Serial.printf("And I counted them all back in again...average flight time =%smS\n",avg);
    } else Serial.printf("Another pigeon home to roost, still waiting for %d\n",nInFlight);
  }
}
// set qos topic names and start HB ticker
void unifiedSetup(){
    addQosToTopic(ctrlQos);
    T1.attach(HEARTBEAT,[]{ Serial.printf("T=%u Heartbeat: Heap=%u number of reconnects=%d\n",millis(), ESP.getFreeHeap(),nRCX); });
}