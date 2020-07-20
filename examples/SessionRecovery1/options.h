#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
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

Ticker mqttReconnectTimer,wifiReconnectTimer,PT1,PT2,PT3;
void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}
void connectToWifi() {
//  WiFi.printDiag(Serial);
  Serial.printf("Connecting to Wi-Fi... SSID=%s\n",WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

#ifdef ARDUINO_ARCH_ESP32
void WiFiEvent(WiFiEvent_t event) {
    Serial.printf("[WiFi-event] event: %d\n", event);
    switch(event) {
    case SYSTEM_EVENT_STA_GOT_IP:
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
        connectToMqtt();
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        Serial.println("WiFi lost connection");
        mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        wifiReconnectTimer.once(RECONNECT_DELAY_W, connectToWifi);
        break;
    }
}
#else
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
#endif

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

#ifdef USE_PANGOLIN
/*
    Necessary error handling absent in asyncMqttClient
*/
void onMqttError(uint8_t e,uint32_t info){
  switch(e){
    case SUBSCRIBE_FAIL:
      Serial.printf("ERROR: SUBSCRIBE_FAIL info=%d\n",info);
      break;
    case INBOUND_QOS_FAIL:
      Serial.printf("ERROR: INBOUND_QOS_FAIL id=%d\n",info);
      break;
    case INBOUND_QOS_ACK_FAIL:
      Serial.printf("ERROR: OUTBOUND_QOS_ACK_FAIL id=%d\n",info);
      break;
    case OUTBOUND_QOS_FAIL:
      Serial.printf("ERROR: OUTBOUND_QOS_FAIL id=%d\n",info);
      break;
    case OUTBOUND_QOS_ACK_FAIL:
      Serial.printf("ERROR: OUTBOUND_QOS_ACK_FAIL id=%d\n",info);
      break;
    case INBOUND_PUB_TOO_BIG:
      Serial.printf("ERROR: INBOUND_PUB_TOO_BIG size=%d Max=%d\n",e,mqttClient.getMaxPayloadSize());
      break;
    case OUTBOUND_PUB_TOO_BIG:
      Serial.printf("ERROR: OUTBOUND_PUB_TOO_BIG size=%d Max=%d\n",e,mqttClient.getMaxPayloadSize());
      break;
    // The BOGUS_xxx messages are 99.99% unlikely to ever happen, but this message is better than a crash, non?
    case BOGUS_PACKET: //  Your server sent a control packet type unknown to MQTT 3.1.1
      Serial.printf("ERROR: BOGUS_PACKET TYPE=%02x\n",e,info);
      break;
    case BOGUS_ACK: // TCP sent an ACK for a packet that we don't remember sending!
    // Only possible causes are: 1) Bug in this lib 2) Bug in ESPAsyncTCP lib 3) Bug in LwIP 4) Your noisy network
    // is SNAFU 5) Subscribing to an invalid name in onMqttConnect
    // Either way, it's pretty fatal, so expect "interesting" results after THIS happens (it won't)
      Serial.printf("ERROR: BOGUS_ACK TCP length=%\n",info);
      break;
    default:
      Serial.printf("UNKNOWN ERROR: %u extra info %d",e,info);
      break;
  }
}
// end error-handling
#endif

#ifdef ARDUINO_ARCH_ESP8266
void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}
void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.printf("Disconnected from Wi-Fi event=%d",event.reason);
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(RECONNECT_DELAY_W, connectToWifi);
}
#endif

#ifdef USE_PANGOLIN
void onMqttDisconnect(int8_t reason) {
#else
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
#endif
  Serial.printf("Disconnected from MQTT reason=%d\n",reason);
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

#ifdef ARDUINO_ARCH_ESP32
  WiFi.onEvent(WiFiEvent);
#else
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
#endif

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
  //  sprintf(buf,"%sDIED",prefix.c_str());
  //  mqttClient.setWill(buf,2,false,"As it very often does"); // different! setWill has a bug
  //
#ifdef USE_PANGOLIN
  mqttClient.setWill((prefix + "DIED").c_str(),2,false,"It's 'Alpha': probably sill some bugs");
  mqttClient.onError(onMqttError);
#else
  //In fact the ONLY thing tht works is:
  //  ( and we know why :) )
  mqttClient.setWill("ADIED",2,false,"As it very often does"); // different! setWill has a bug
#endif

  mqttClient.setCleanSession(START_WITH_CLEAN_SESSION);
  mqttClient.setKeepAlive(RECONNECT_DELAY_M *3);
#ifdef USE_TLS
  mqttClient.serverFingerprint(cert);
  //mqttClient.setCredentials("your username","your password"); // if required
#endif

  connectToWifi();
  unifiedSetup();
}

void loop() {}