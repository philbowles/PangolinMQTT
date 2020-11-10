#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <Ticker.h>
/*
 * This file "abstracts away" the automatic connection / reconnection sequence
 * and is used by all sketches so that only the core of the MQTT functionality is visible
 * in each example  sketch, making it easier to "see the wood for the trees"
 * 
 * It provides:
 * a) A "framework" to connect / disconnect to wifi / mqtt
 * b) A selection of utility functions to reduce user effort and allow
 *    simple payload handling in the main sketch, irrespective of content type
 * c) Three additional user timers
 * d) a default error handler   
*/
#include <PangolinMQTT.h> 
#define LIBRARY "PangolinMQTT "PANGO_VERSION
PangolinMQTT mqttClient;

#if ASYNC_TCP_SSL_ENABLED
#define MQTT_PORT 8883
#else
#define MQTT_PORT 1883
#endif

#define RECONNECT_DELAY_M   5
#define RECONNECT_DELAY_W   5

Ticker mqttReconnectTimer,wifiReconnectTimer,PT1,PT2,PT3;

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}
void connectToWifi() {
    Serial.printf("Connecting to Wi-Fi... SSID=%s\n",WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

#ifdef ARDUINO_ARCH_ESP32
void WiFiEvent(WiFiEvent_t event) {
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

extern void userSetup();
extern void onMqttConnect(bool);
extern void onMqttMessage(const char* topic, const uint8_t* payload, size_t len,uint8_t qos,bool retain,bool dup);

extern const char* mqAuth;
extern const char* mqPass;
/*
    default error handling
*/

void onMqttError(uint8_t e,uint32_t info){
  switch(e){
    case TCP_DISCONNECTED:
        // usually because your structure is wrong and you called a function before onMqttConnect
        Serial.printf("ERROR: NOT CONNECTED info=%d\n",info);
        break;
    case MQTT_SERVER_UNAVAILABLE:
        // server has gone away - network problem? server crash?
        Serial.printf("ERROR: MQTT_SERVER_UNAVAILABLE info=%d\n",info);
        break;
    case UNRECOVERABLE_CONNECT_FAIL:
        // there is something wrong with your connection parameters? IP:port incorrect? user credentials typo'd?
        Serial.printf("ERROR: UNRECOVERABLE_CONNECT_FAIL info=%d\n",info);
        break;
    case TLS_BAD_FINGERPRINT:
        Serial.printf("ERROR: TLS_BAD_FINGERPRINT info=%d\n",info);
        break;
    case SUBSCRIBE_FAIL:
        // you tried to subscribe to an invalid topic
        Serial.printf("ERROR: SUBSCRIBE_FAIL info=%d\n",info);
        break;
    case INBOUND_QOS_ACK_FAIL:
        Serial.printf("ERROR: OUTBOUND_QOS_ACK_FAIL id=%d\n",info);
        break;
    case OUTBOUND_QOS_ACK_FAIL:
        Serial.printf("ERROR: OUTBOUND_QOS_ACK_FAIL id=%d\n",info);
        break;
    case INBOUND_PUB_TOO_BIG:
        // someone sent you a p[acket that this MCU does not have enough FLASH to handle
        Serial.printf("ERROR: INBOUND_PUB_TOO_BIG size=%d Max=%d\n",e,mqttClient.getMaxPayloadSize());
        break;
    case OUTBOUND_PUB_TOO_BIG:
        // you tried to send a packet that this MCU does not have enough FLASH to handle
        Serial.printf("ERROR: OUTBOUND_PUB_TOO_BIG size=%d Max=%d\n",e,mqttClient.getMaxPayloadSize());
        break;
    case BOGUS_PACKET: //  Your server sent a control packet type unknown to MQTT 3.1.1 
    //  99.99% unlikely to ever happen, but this message is better than a crash, non? 
        Serial.printf("ERROR: BOGUS_PACKET TYPE=%02x\n",e,info);
        break;
    case X_INVALID_LENGTH: //  An x function rcvd a msg with an unexpected length: probale data corruption or malicious msg 
    //  99.99% unlikely to ever happen, but this message is better than a crash, non? 
        Serial.printf("ERROR: X_INVALID_LENGTH TYPE=%02x\n",e,info);
        break;
    default:
      Serial.printf("UNKNOWN ERROR: %u extra info %d",e,info);
      break;
  }
}
// end error-handling

#ifdef ARDUINO_ARCH_ESP8266
void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.print("Connected to Wi-Fi as ");
  Serial.println(WiFi.localIP());
  connectToMqtt();
}
void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.printf("Disconnected from Wi-Fi event=%d\n",event.reason);
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(RECONNECT_DELAY_W, connectToWifi);
}
#endif

extern void onMqttDisconnect(int8_t reason);

void setup() {
  Serial.begin(115200);
  delay(250); //why???
  Serial.printf("%s Starting heap=%u\n",LIBRARY,ESP.getFreeHeap());
  
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
  mqttClient.setWill("DIED",2,false,"probably still some bugs");
  mqttClient.onError(onMqttError);
  mqttClient.setCleanSession(START_WITH_CLEAN_SESSION);
  mqttClient.setKeepAlive(RECONNECT_DELAY_M *3);
  if(mqAuth!="") mqttClient.setCredentials(mqAuth,mqPass);
#if ASYNC_TCP_SSL_ENABLED
  mqttClient.serverFingerprint(cert);
#endif
  
  connectToWifi();
  userSetup();
}