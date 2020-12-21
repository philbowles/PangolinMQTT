![plainhdr](../assets/pangoplain.jpg)

# Callbacks


```cpp
// M A N D A T O R Y ! ! !
void cbConnect(bool session); // session = whether connection has started with a dirty session
// M A N D A T O R Y  if subscribing:
void cbMessage(const char* topic, const uint8_t* payload, size_t len,uint8_t qos,bool retain,bool dup);
// O P T I O N A L
void cbDisconnect(int8_t reason);
/*
LwIP produces [error codes](https://www.nongnu.org/lwip/2_0_x/group__infrastructure__errors.html) with negative value and these get fed back up through ESPAsyncTCPeventually to this library, which also has a few of its own valid reasons. How to tell the difference? All of Pangolin's are +ve, but if you get a rare underlying TCP error which will help in diagnosing problems, you will also get told that (-ve) reason code
*/
void cbError(uint8_t error,int info); // info is additional information about the error whose code is one of:
/*
TCP_DISCONNECTED, // usually because your program structure is wrong, e.g. you called publish when you arent connected
MQTT_SERVER_UNAVAILABLE, // usually when server crashes or times out
UNRECOVERABLE_CONNECT_FAIL, // usually your credentials are incorrect
TLS_BAD_FINGERPRINT, // SHA1 does not match server certificate's fingerprint
SUBSCRIBE_FAIL, // invalid topic provided
INBOUND_QOS_ACK_FAIL, // an ID has been provided by the server that is no longer held by us (usually after crash/reboot with open session)
OUTBOUND_QOS_ACK_FAIL,// an ID has been provided by the server that is no longer held by us (usually after crash/reboot with open session)
INBOUND_PUB_TOO_BIG, // someone sent you a waaaaaay too big message
OUTBOUND_PUB_TOO_BIG, // you tried to send out a a waaaaaay too big message
BOGUS_PACKET, // should never happen - server sent malformed / unrecognised packet - SERIOUS PROBLEM
X_INVALID_LENGTH // length of payload does not match expected data type in x functions - server sent malformed message - SERIOUS PROBLEM
*/

```

---

# API

```cpp
void connect();
void disconnect(bool force = false);

const char* getClientId();
size_t getMaxPayloadSize();

void onConnect(PANGO_cbConnect callback); // mandatory: set connect handler
void onDisconnect(PANGO_cbDisconnect callback);// optional: set disconnect handler
void onError(PANGO_cbError callback);// optional: set error handler
void onMessage(PANGO_cbMessage callback); // // mandatory if subscribing: set topic handler

void publish(const char* topic,const uint8_t* payload, size_t length, uint8_t qos=0,  bool retain=false);
void publish(const char* topic,const char* payload, size_t length, uint8_t qos=0,  bool retain=false);
template<typename T>
void publish(const char* topic,T v,const char* fmt="%d",uint8_t qos=0,bool retain=false);

void serverFingerprint(const uint8_t* fingerprint); // only if using TLS
void setCleanSession(bool cleanSession); // optional, default is clean session
void setClientId(const char* clientId); // optional if you want to control your own device name
void setCredentials(const char* username, const char* password = nullptr); // optional if your server requires them
void setKeepAlive(uint16_t keepAlive); // probably best left alone... note actual rate is PANGO_POLL_RATE * keepAlive; and depends on your LwIP
void setServer(IPAddress ip, uint16_t port); 
void setServer(const char* host, uint16_t port);
void setWill(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr); // optional

void subscribe(const char* topic, uint8_t qos);
void unsubscribe(const char* topic);

void xPublish(const char* topic,const char* value, uint8_t qos=0,  bool retain=false);
void xPublish(const char* topic,String value, uint8_t qos=0,  bool retain=false);
void xPublish(const char* topic,std::string value, uint8_t qos=0,  bool retain=false);
template<typename T>
void xPublish(const char* topic,T value, uint8_t qos=0,  bool retain=false)

void xPayload(const uint8_t* payload,size_t len,char*& cp); // YOU MUST FREE THE POINTER CREATED BY THIS CALL!!!
void xPayload(const uint8_t* payload,size_t len,std::string& ss);
void xPayload(const uint8_t* payload,size_t len,String& duino);
template<typename T>
void xPayload(const uint8_t* payload,size_t len,T& value);

```

---

# Advanced Topics

PangolinMQTT comes with some built-in diagnostics. These can be controlled by setting the debug level in `config.h`

```cpp
/*
    Debug levels: 
    0 - No debug messages
    1 - connection / disconnection messages
    2 - level 1 + MQTT packet types
    3 - level 2 + MQTT packet data
    4 - everything
*/

#define PANGO_DEBUG 1
```

You can also include your own using `PANGO_PRINTx` functions (where x is 1-4).
These operate just like `printf` but will only be compiled-in if the PANGO_DEBUG level is x or greater

```cpp
PANGO_PRINT3("FATAL ERROR %d\n",errorcode); 
```

Will only be compiled if user sets PANGO_DEBUG to 3 or above



---

## Find me daily in these FB groups

* [Pangolin Support](https://www.facebook.com/groups/pangolinmqtt/)
* [ESP8266 & ESP32 Microcontrollers](https://www.facebook.com/groups/2125820374390340/)
* [ESP Developers](https://www.facebook.com/groups/ESP8266/)
* [H4/Plugins support](https://www.facebook.com/groups/h4plugins)

I am always grateful for any $upport on [Patreon](https://www.patreon.com/esparto) :)


(C) 2020 Phil Bowles
