/*
 * PLEASE READ THE NOTES ON THIS SKETCH FIRST AT
 * 
 * https://github.com/philbowles/pangolin/
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
#define START_WITH_CLEAN_SESSION   true

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
// Default values:
#define QOS 0
#define PAYLOAD_SIZE       100
#define TRANSMIT_RATE     1000
#define AVOID_SPIKE_PCENT   90
#define HEARTBEAT           10
// NB RATE IS IN MILLISECONDS!!!!
//
std::string tofTopic=uTopic("tof"); // the out-and-back message
std::string cmaTopic=uTopic("cma");// the Continuous moving average of the T-O-F (always Qos0)

uint8_t ctrlQos=QOS; // can be changed in-flight by publishing "qos" with payload 0 1 or 2
uint32_t ctrlRate=TRANSMIT_RATE; // can be changed in-flight by publishing "rate" with payload of new TX rate
uint32_t ctrlSize=PAYLOAD_SIZE; // can be changed in-flight by publishing "size" with payload of new T-O-F size

// publish this from any client to change qos of round-trip packets
// the size payload set to the new size of the T-O-F payload
std::string ctrlTopicQ=("qos");
// publish this from any client to change TX rate
// the size payload set to the new period in seconds
std::string ctrlTopicR=("rate");
// publish this from any client to change size of T-O-F payload
// the size payload set to the new size of the T-O-F payload
std::string ctrlTopicS=("size");
// publish this from any client to show current values
std::string ctrlTopicV=("show");
// publish this from any client to pause sending
std::string ctrlTopicX=("stop");
// publish this from any client to start sending
std::string ctrlTopicY=("start");

int  cma=0;  // cumulative moving average;
int  N=0;    // no of sends

void setTofQosTopic(uint8_t qos){
    tofTopic.pop_back(); // trim qos from topic
    cmaTopic.pop_back(); // trim qos from topic
    tofTopic.push_back(qos + 0x30); // add back on new Qos 
    cmaTopic.push_back(qos + 0x30); // add back on new Qos 
}

void showValues(){
  Serial.printf("Using QoS%d with payload size %d rate every %d.%03d second(s)\n",ctrlQos,ctrlSize,ctrlRate/1000,ctrlRate%1000);    
  Serial.printf("Round-trip TOF=%s, CMA=%s Smoothing +/-%d%%\n",tofTopic.c_str(),cmaTopic.c_str(),AVOID_SPIKE_PCENT);    
}

void sendTOF(){
  char* buf=static_cast<char*>(malloc(ctrlSize)); // allocate the payload buffer
  for(int i=0;i<ctrlSize;i++) buf[i]='X'; // fill it with known values
  sprintf(buf,"%d",millis()); // put tof in first part
  Serial.printf("WTOF? %s n=%d\n",buf,strlen(buf));
  unifiedPublish(tofTopic.c_str(), ctrlQos, false, (uint8_t*) buf, ctrlSize);
  free(buf);
}

void addQosToTopic(uint8_t qos){
   tofTopic.push_back(qos + 0x30); // add on initial Qos e.g. Ptof and qos=1 becomes Ptof1
   cmaTopic.push_back(qos + 0x30); // add on initial Qos e.g. Atof and qos=0 becomes Atof0   
}

void changeValues(uint8_t newQos,uint32_t newRate,uint32_t newSize){
  Serial.printf("CHANGE Q=%d R=%d S=%d\n",newQos,newRate,newSize);
  if(newQos!=ctrlQos){
    Serial.printf("Changing from QoS%d to QoS%d\n",ctrlQos,newQos);
    Serial.printf("Unsubscribe from %s\n",tofTopic.c_str());
    unifiedUnsubscribe(tofTopic.c_str()); // lose old sub
    tofTopic.pop_back(); // trim qos from topic
    cmaTopic.pop_back(); // trim qos from topic
    addQosToTopic(newQos);
    ctrlQos=newQos;
    Serial.printf("Subscribe to %s @ QoS%d\n",tofTopic.c_str(),ctrlQos);
    unifiedSubscribe(tofTopic.c_str(),ctrlQos); // T-O-F topic @ chosen QoS   
  }
  if(newRate!=ctrlRate){
    Serial.printf("Changing from rate %dmS to %dmS\n",ctrlRate,newRate);
    T2.detach();
    T2.attach_ms(newRate,sendTOF);
    ctrlRate=newRate;
  }
  if(newSize!=ctrlSize){
    Serial.printf("Changing from payload size of %dB to %dB\n",ctrlSize,newSize);
    ctrlSize=newSize;
  }
  cma=N=0; // reset values after change so no "bleed-over"
  showValues();
}

void unifiedMqttConnect() {
  unifiedSubscribe(ctrlTopicQ.c_str(),0);
  unifiedSubscribe(ctrlTopicR.c_str(),0);
  unifiedSubscribe(ctrlTopicS.c_str(),0);
  unifiedSubscribe(ctrlTopicV.c_str(),0);
  unifiedSubscribe(ctrlTopicX.c_str(),0);
  unifiedSubscribe(ctrlTopicY.c_str(),0);
  
  unifiedSubscribe(tofTopic.c_str(),ctrlQos); // T-O-F topic @ chosen QoS   
  
  showValues();
  T2.attach_ms(ctrlRate,sendTOF);
}

void unifiedMqttDisconnect(int8_t reason) {
  Serial.printf("USER: Disconnect reason=%d\n",reason);
  T2.detach();
}

void unifiedMqttMessage(std::string topic, uint8_t* payload, uint8_t qos, bool dup, bool retain, size_t len, size_t index, size_t total) {
  Serial.printf("WTF? PL=%08X len=%d i=%d t=%d qos=%d\n",payload,len,index,total,qos);
  PANGO::dumphex(payload,len);
  if(topic=="qos"){
    uint8_t newqos=payloadToInt(payload,len); 
    if(newqos < 3) changeValues(newqos,ctrlRate,ctrlSize);
    else Serial.printf("ERROR! Cannot change to QoS%d!!!\n",newqos);
  }
  else if(topic=="rate") {
    uint32_t newrate=payloadToInt(payload,len); 
    if(newrate > 125 && newrate < 10000) changeValues(ctrlQos,newrate,ctrlSize); // arbitrary "sensible" values
    else Serial.printf("ERROR! Rate shuld be between 125 and 10000 (0.125s and 10s) %d is not!!!\n",newrate);
  }
  else if(topic=="size") {
    uint32_t newsize=payloadToInt(payload,len); 
    if(newsize > 16) changeValues(ctrlQos, ctrlRate,newsize);  // at least a string value of a massive integer + some safety
    else Serial.printf("ERROR! Payload size %d is too small!!!\n",newsize);
  }
  else if(topic=="stop") {
    Serial.printf("Sending Stopped\n");
    T2.detach();
  }
  else if(topic=="start") {
    Serial.printf("Sending Started\n");
    T2.attach_ms(ctrlRate,sendTOF);
  }
  else if(topic=="show") { showValues(); }
  else { // default tof topic
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
    //
    int tof=millis() - payloadToInt(payload,len); // => now - tod = tof
    // exclude massive spikes > x%
    if(!N || ((abs(tof - cma)) < (((cma) * AVOID_SPIKE_PCENT) / 100 ))){
      int old=cma;
      cma=old+((tof-old)/(++N));
      Serial.printf("PL=%d i=%d t=%d QoS%d N=%d tof=%d Cumulative Moving Avg=%d\n",len,index,total,qos,N,tof,cma);
      char scma[16];
      sprintf(scma,"%d",cma);
      // always publish cma vales at qos0
      unifiedPublish(cmaTopic.c_str(), 0, false, (uint8_t*) scma, strlen(scma));
    }
    else Serial.printf("Spike of %d excluded\n",tof);
  }
}
// set qos topic names and start HB ticker
void unifiedSetup(){
    addQosToTopic(ctrlQos);
    T1.attach(HEARTBEAT,[]{ Serial.printf("T=%u Heartbeat: Heap=%u number of reconnects=%d\n",millis(), ESP.getFreeHeap(),nRCX); });
}
