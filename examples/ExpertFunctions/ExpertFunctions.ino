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
 
const char    wrong[] = "multi-line payload hex dumper which should split over several lines, with some left over";
const uint8_t right[] = "multi-line payload hex dumper which should split over several lines, with some left over";
#define RIGHT_SIZE 88

void onMqttConnect(bool sessionPresent) {
  Serial.printf("Connected to MQTT session=%d max payload size=%d\n",sessionPresent,mqttClient.getMaxPayloadSize());
  Serial.println("Subscribing at QoS 2");
  mqttClient.subscribe("test/#", 2);
//
//  the x files - sorry, "functions"
//
//  numbery stuff
  uint32_t u32=0xdeadbeef;
  uint16_t u16=0xf1f0;
  uint8_t u8=0x8;
  int i=666;
  bool b=true;
  float f=3.14159;
  Serial.printf("Publishing uint32_t 0x%x\n",u32);
  mqttClient.xPublish("test/uint32",u32);
  Serial.printf("Publishing uint16_t 0x%x\n",u16);
  mqttClient.xPublish("test/uint16",u16);
  Serial.printf("Publishing uint8_t 0x%x\n",u8);
  mqttClient.xPublish("test/uint8",u8);
  Serial.printf("Publishing int %d\n",i);
  mqttClient.xPublish("test/int",i);
  Serial.printf("Publishing bool %s\n",b ? "true":"false");
  mqttClient.xPublish("test/bool",b);
  uint32_t u=f*100000;
  Serial.printf("Publishing float %d.%05d\n",u/100000,u%100000); // upscale 5dp, no %f operator
  mqttClient.xPublish("test/float",f);
// stringy stuff
// wrong already declared
  std::string ss("std:string containing some data");
  String duino("Arduino String with some stuff in it");
  Serial.printf("Publishing pchar %s l=%d\n",wrong,strlen(wrong));
  mqttClient.xPublish("test/pchar",wrong);
  Serial.printf("Publishing std::string %s l=%d\n",ss.c_str(),ss.size());
  mqttClient.xPublish("test/ss",ss);
  Serial.printf("Publishing Arduino String %s l=%d\n",duino.c_str(),duino.length());
  mqttClient.xPublish("test/duino",duino);
}

void onMqttMessage(const char* topic, const uint8_t* payload, size_t len,uint8_t qos,bool retain,bool dup) {
  // now do intelligent unpack:
  // 1) strip out subtopic
  std::string t(topic);
  std::string sub = t.substr(t.find("/")+1);
  Serial.printf("H=%u Message %s qos%d dup=%d retain=%d len=%d\n",ESP.getFreeHeap(),topic,qos,dup,retain,len);
  PANGO::dumphex(payload,len,16);
  // 2) call appropriate variant of xPayload depending on expected type
  Serial.printf("Unpack %s ",sub.c_str());
  if(sub=="uint32"){
    uint32_t v;
    mqttClient.xPayload(payload,len,v);
    Serial.printf("0x%x\n",v);
  } else if(sub=="uint16"){
    uint16_t v;
    mqttClient.xPayload(payload,len,v);
    Serial.printf("0x%x\n",v);
  } else if(sub=="uint8"){
    uint8_t v;
    mqttClient.xPayload(payload,len,v);
    Serial.printf("0x%x\n",v);
  } else if(sub=="int"){
    int v;
    mqttClient.xPayload(payload,len,v);
    Serial.printf("%d\n",v);
  } else if(sub=="bool"){
    bool v;
    mqttClient.xPayload(payload,len,v);
    Serial.printf("%s\n",v ? "true":"false");    
  } else if(sub=="float"){ // scale to 2dp, no builtin %f operator! 
    float f;
    mqttClient.xPayload(payload,len,f);
    uint32_t v=100*f;
    Serial.printf("%d.%2d\n",v/100,v%100);    
  } else if(sub=="pchar"){
    char* buf;
    mqttClient.xPayload(payload,len,buf);
    Serial.printf("%s l=%d\n",buf,strlen(buf));
    free(buf); // DO NOT FORGET TO DO THIS!!!    
  } else if(sub=="ss"){
    std::string buf;
    mqttClient.xPayload(payload,len,buf);
    Serial.printf("%s l=%d\n",buf.c_str(),buf.size());
  } else if(sub=="duino"){
    String buf;
    mqttClient.xPayload(payload,len,buf);
    Serial.printf("%s l=%d\n",buf.c_str(),buf.length());
  } else Serial.printf("????\n");
}

void userSetup() {
// nothing to do
}

void loop() {}