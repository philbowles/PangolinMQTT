#include <PangolinMQTT.h> 
PangolinMQTT mqttClient;

#include <Ticker.h>

Ticker PT1;
//
// if using TLS / https, edit pango_config.h and #define ASYNC_TCP_SSL_ENABLED 1
// do the same in async_config.h of the PATCHED ESPAsyncTCP library!! 

//#define MQTT_URL "https://192.168.1.21:8883"
#define MQTT_URL "http://192.168.1.21:1883"
const uint8_t* cert=nullptr;

// if you provide a valid certificate when connecting, it will be checked and fail on no match
// if you do not provide one, PangolinMQTT will continue insecurely with a warning
// this one is MY local mosquitto server... it ain't gonna work, so either don't use one, or set your own!!!
//const uint8_t cert[20] = { 0x9a, 0xf1, 0x39, 0x79,0x95,0x26,0x78,0x61,0xad,0x1d,0xb1,0xa5,0x97,0xba,0x65,0x8c,0x20,0x5a,0x9c,0xfa };
//#define MQTT_URL "https://robot.local:8883"

// If using MQTT server authentication, fill in next two fields!
const char* mqAuth="example";
const char* mqPass="pangolin";
//
//  Some sketches will require you to set START_WITH_CLEAN_SESSION to false
//  For THIS sketch, leave it at false
//
#define START_WITH_CLEAN_SESSION   true

const char* pload0="multi-line payload hex dumper which should split over several lines, with some left over";
const char* pload1="PAYLOAD QOS1";
const char* pload2="Save the Pangolin!";

void onMqttError(uint8_t e,uint32_t info){
  Serial.printf("VALUE OF TCP_DISCONNECTED=%d\n",TCP_DISCONNECTED);
  switch(e){
    case VARK_TCP_DISCONNECTED:
        Serial.printf("ERROR: NOT CONNECTED info=%d\n",info);
        break;
    case VARK_TCP_UNHANDLED:
        Serial.printf("ERROR: UNHANDLED TCP ERROR info=%d\n",info);
        break;
    case VARK_TLS_BAD_FINGERPRINT:
        Serial.printf("ERROR: TLS_BAD_FINGERPRINT info=%d\n",info);
        break;
    case VARK_TLS_NO_FINGERPRINT:
        Serial.printf("WARNING: NO FINGERPRINT, running insecure\n");
        break;
    case VARK_TLS_UNWANTED_FINGERPRINT:
        Serial.printf("WARNING: FINGERPRINT provided, insecure http:// given\n");
        break;
    case VARK_TLS_NO_SSL:
        Serial.printf("ERROR: secure https:// requested, NO SSL COMPILED-IN: READ DOCS!\n");
        break;
    case VARK_NO_SERVER_DETAILS: //  
    //  99.99% unlikely to ever happen, make sure you call setServer before trying to connect!!!
        Serial.printf("ERROR:NO_SERVER_DETAILS info=%02x\n",info);
        break;
    case VARK_INPUT_TOO_BIG: //  
    //  99.99% unlikely to ever happen, make sure you call setServer before trying to connect!!!
        Serial.printf("ERROR: RX msg(%d) that would 'break the bank'\n",info);
        break;    
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
        if(info==5) Serial.printf("MQTT Authetication Error\n",info);
        break;
    case TLS_BAD_FINGERPRINT:
        Serial.printf("ERROR: TLS_BAD_FINGERPRINT info=%d\n",info);
        break;
    case TLS_NO_FINGERPRINT:
        Serial.printf("WARNING: NO FINGERPRINT, running insecure\n");
        break;
    case TLS_UNWANTED_FINGERPRINT:
        Serial.printf("WARNING: FINGERPRINT provided, insecure http:// given\n");
        break;
    case TLS_NO_SSL:
        Serial.printf("ERROR: secure https:// requested, NO SSL COMPILED-IN: READ DOCS!\n");
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
        Serial.printf("ERROR: INBOUND_PUB_TOO_BIG size=%d Max=%d\n",info,mqttClient.getMaxPayloadSize());
        break;
    case OUTBOUND_PUB_TOO_BIG:
        // you tried to send a packet that this MCU does not have enough FLASH to handle
        Serial.printf("ERROR: OUTBOUND_PUB_TOO_BIG size=%d Max=%d\n",info,mqttClient.getMaxPayloadSize());
        break;
    case BOGUS_PACKET: //  Your server sent a control packet type unknown to MQTT 3.1.1 
    //  99.99% unlikely to ever happen, but this message is better than a crash, non? 
        Serial.printf("ERROR: BOGUS_PACKET info=%02x\n",info);
        break;
    case X_INVALID_LENGTH: //  An x function rcvd a msg with an unexpected length: probale data corruption or malicious msg 
    //  99.99% unlikely to ever happen, but this message is better than a crash, non? 
        Serial.printf("ERROR: X_INVALID_LENGTH info=%02x\n",info);
        break;
    case NO_SERVER_DETAILS: //  
    //  99.99% unlikely to ever happen, make sure you call setServer before trying to connect!!!
        Serial.printf("ERROR:NO_SERVER_DETAILS info=%02x\n",info);
        break;
    default:
      Serial.printf("UNKNOWN ERROR: %u extra info %d\n",e,info);
      break;
  }
}

void onMqttConnect(bool session) {
  Serial.printf("USER: Connected as %s session=%d max payload size=%d\n",mqttClient.getClientId().data(),session,mqttClient.getMaxPayloadSize());

  Serial.println("USER: Subscribing at QoS 2");
  mqttClient.subscribe({"test","multi2","fully/compliant"}, 2);
  Serial.printf("USER: T=%u Publishing at QoS 0\n",millis());
  mqttClient.publish("test",pload0,strlen(pload0));
  Serial.printf("USER: T=%u Publishing at QoS 1\n",millis());
  mqttClient.publish("test",pload1,strlen(pload1),1); 
  Serial.printf("USER: T=%u Publishing at QoS 2\n",millis());
  mqttClient.publish("test",pload2,strlen(pload2),2);

  PT1.attach(10,[]{
    // simple way to publish int types  as strings using printf format
    mqttClient.publish("test",_HAL_freeHeap(),"%u"); 
    mqttClient.publish("test",-33); 
  });

  mqttClient.unsubscribe({"multi2","fully/compliant"});
}

void onMqttMessage(const char* topic, const uint8_t* payload, size_t len,uint8_t qos,bool retain,bool dup) {
  Serial.printf("\nUSER: H=%u Message %s qos%d dup=%d retain=%d len=%d\n",ESP.getFreeHeap(),topic,qos,dup,retain,len);
  dumphex(payload,len);
  Serial.println();
}

void onMqttDisconnect(int8_t reason) {
  Serial.printf("USER: Disconnected from MQTT reason=%d\n",reason);
}

void setup(){
  Serial.begin(115200);
  delay(250); //why???
  Serial.printf("\nPangolinMQTT v%s running @ debug level %d heap=%u\n",PANGO_VERSION,PANGO_DEBUG,ESP.getFreeHeap()); 
  
  mqttClient.onMqttError(onMqttError);
  mqttClient.onMqttConnect(onMqttConnect);
  mqttClient.onMqttDisconnect(onMqttDisconnect);
  mqttClient.onMqttMessage(onMqttMessage);
  mqttClient.setServer(MQTT_URL,mqAuth,mqPass,cert);
  mqttClient.setWill("DIED",2,false,"probably still some bugs");
//  mqttClient.setKeepAlive(RECONNECT_DELAY_M *3); // very rarely need to change this (if ever)

  WiFi.begin("XXXXXXXX","XXXXXXXX");
  while(WiFi.status()!=WL_CONNECTED){
    Serial.print(".");
    delay(1000);
  }
  
  Serial.printf("WIFI CONNECTED IP=%s\n",WiFi.localIP().toString().c_str());

  mqttClient.connect("Pangolin_101",START_WITH_CLEAN_SESSION);
}

void loop() {}
