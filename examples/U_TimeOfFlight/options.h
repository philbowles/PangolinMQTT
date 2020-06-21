#include <ESP8266WiFi.h>
#include <Ticker.h>
/*
 * This file "irons out" the differences between Pangolin API and AsncMqttClient API
 * and hides the incorrcet and illogal parametrs of the latter from the user
 * 
 * It provides:
 * 
 * a) A common "framework" to connect / disconnect to wifi / mqtt
 * b) A selection fo utility functions to reduce user effort and allow
 *    simple payload handling in the main sketch, irrespective of content type
 * b) a unified API that works with either lib so that the main code in the sketch 
 *    will compile cleanly for either library
 *    
 * It also hides all the "machinery" so that the porpuse and intent of the main sketch
 * is clear and obvious as it is reduced to two simple functions:
 * 
 *  unifiedMqttConnect  when mqtt connects, in whch the user should then...
 *    unifiedSubscribe( topic , qos); // as per usua: sme parameters as "normal" subscribe
 *    
 *  unifiedMqttMesage to handle incoming messages in the same wasy as the "normal" onMqttConnect
 *    (but with correct data types and less BS)
 *  
 *  at any point, user can call unifiedPublish with similar parameters to the original,
 *    (but with correct data types and less BS)
 *    
 *  Finally for anyskecth that need to set global variables of timers etc, there is
 *  
 *  unifiedSetup
 *  
 */
#ifdef USE_PANGOLIN
  #define LIBRARY "Pangolin v0.0.7"
  #pragma message("Compiling for Pangolin")
  #include <PangolinMQTT.h> 
  PangolinMQTT mqttClient;
#else
  #define LIBRARY "AsyncMqttClient v0.8.2"
  #pragma message("Compiling for AsyncMqttClient")
  #include <AsyncMqttClient.h> 
  AsyncMqttClient mqttClient;
#endif

#define RECONNECT_DELAY_M   5
#define RECONNECT_DELAY_W   5

uint32_t  nRCX=0;

std::string lib(LIBRARY);
std::string prefix(lib.begin(),++lib.begin());

Ticker mqttReconnectTimer,wifiReconnectTimer,T1,T2,T3;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

extern void unifiedSetup();
extern void unifiedMqttConnect();
extern void unifiedMqttMessage(std::string,uint8_t*,uint8_t,bool,bool,size_t,size_t,size_t);
extern void unifiedMqttDisconnect(int8_t);

void unifiedSubscribe(std::string topic,uint8_t qos){ 
  Serial.printf("unifiedSubscribe %s @ QoS%d\n",topic.c_str(),qos);
  mqttClient.subscribe(topic.c_str(),qos);
}

void unifiedPublish(std::string t,uint8_t q,bool r,uint8_t* p,size_t l){
#ifdef USE_PANGOLIN
  mqttClient.publish(t.c_str(),q,r,p,l,false); 
#else
  mqttClient.publish(t.c_str(),q,r,(char*) p,l); 
#endif
}
void unifiedUnsubscribe(std::string topic){ mqttClient.unsubscribe(topic.c_str()); }

std::string uTopic(std::string t){ return (prefix+t); } // automatically prefixes the topic with "A" or "P"

void onMqttConnect(bool sessionPresent) {
  nRCX++;
#ifdef USE_PANGOLIN
  Serial.printf("Connected to MQTT: Session=%d Max safe payload %u\n",sessionPresent,mqttClient.getMaxPayloadSize());
#else
  Serial.printf("Connected to MQTT: Session=%d FIXED\n",sessionPresent);
#endif
  unifiedMqttConnect();
} 
/* 
 *  utility functions missing from AsyncMqttClient but present in Pangolin
 *  extracted from PANGO:: namespace and reproduced here as static globals 
 *  so AsyncMqttClient examples can use identical code
 *  
 */
char* payloadToCstring(uint8_t* data,size_t len){
    char* buf=static_cast<char*>(malloc(len+1)); /// CALLER MUST FREE THIS!!!
    memcpy(buf,data,len);
    buf[len]='\0';
    return buf;
};

int payloadToInt(uint8_t* data,size_t len){
    char* c=payloadToCstring(data,len);
    int i=atoi(c);
    free(c); // as all goood programmers MUST!
    return i;
}
std::string payloadToStdstring(uint8_t* data,size_t len){
    char* c=payloadToCstring(data,len);
    std::string s;
    s.assign(c,len);
    free(c); // as all goood programmers MUST!
    return s;
}
const char* cstringFromInt(int i){
  static char buf[32]; // lazy but safe
  sprintf(buf,"%d",i);
  return buf;
}

void dumphex(const void *mem, uint32_t len, uint8_t cols=16) {
    const uint8_t* src = (const uint8_t*) mem;
    Serial.printf("Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
    for(uint32_t i = 0; i < len; i++) {
        if(i % cols == 0) Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
        Serial.printf("%02X ", *src);
        src++;
    }
    Serial.printf("\n");
}
// end utils


void connectToWifi() {
  WiFi.printDiag(Serial);
  Serial.printf("Connecting to Wi-Fi... SSID=%s\n",WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.printf("Disconnected from Wi-Fi event=%d",event.reason);
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(RECONNECT_DELAY_W, connectToWifi);
}

#ifdef USE_PANGOLIN
void onMqttDisconnect(int8_t reason) {
#else
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
#endif
  Serial.printf("Disconnected from MQTT reason=%d\n",reason);
  T2.detach();
  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(RECONNECT_DELAY_M, connectToMqtt);
  }
  unifiedMqttDisconnect((int8_t) reason);
}
#ifdef USE_PANGOLIN
void onMqttMessage(const char* top,uint8_t* p,struct PANGO_PROPS pp,size_t l,size_t i,size_t tot){
  unifiedMqttMessage(std::string(top),p,pp.qos,pp.dup,pp.retain,l,i,tot);
}
#else
void onMqttMessage(char* top, char* p, AsyncMqttClientMessageProperties pp, size_t l, size_t i, size_t tot) {
  unifiedMqttMessage(std::string(top),(uint8_t*)p,pp.qos,pp.dup,pp.retain,l,i,tot);  
}
#endif

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.printf("%s Starting heap=%u\n",LIBRARY,ESP.getFreeHeap());

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  //DIFFERENT TO AVOID setWill bug: DCX/CNX loop
  //Prefectly correct C/C++ Pangolin code fails when used by AsyncMqttClient:
  //  mqttClient.setWill((prefix + "DIED").c_str(),2,false,"DIED");
  // 
  //The following bog-standard C also fails
  //  char buf[6];
  //  sprintf(buf,"%sDIED",pfx.c_str());
  //  mqttClient.setWill(buf,2,false,"As it very often does"); // different! setWill has a bug
  //
#ifdef USE_PANGOLIN
  mqttClient.setWill((prefix + "DIED").c_str(),2,false,"It's 'Alpha': probably sill some bugs");
#else
  //In fact the ONLY thing tht works is:
  //  ( and we know why :) )
  mqttClient.setWill("ADIED",2,false,"As it very often does"); // different! setWill has a bug
#endif  
  
  mqttClient.setCleanSession(START_WITH_CLEAN_SESSION);
  mqttClient.setKeepAlive(RECONNECT_DELAY_M *3);
  
  connectToWifi();

  unifiedSetup();
}

void loop() {}