/*
MIT License

Copyright (c) 2020 Phil Bowles with huge thanks to Adam Sharp http://threeorbs.co.uk
for testing, debugging, moral support and permanent good humour.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once

#include"pango_config.h"

#include<Arduino.h>

#include<functional>
#include<string>
#include<map>
#include<queue>

#ifdef ARDUINO_ARCH_ESP32
    #include <AsyncTCP.h> /// no tls yet
#else
    #include <ESPAsyncTCP.h>
    #if ASYNC_TCP_SSL_ENABLED
        #include <tcp_axtls.h>
    #endif
#endif

const char*_HAL_getUniqueId();

#include<AardvarkTCP.h>
#include<AardvarkUtils.h>

enum PANGO_FAILURE : uint8_t {
    TCP_DISCONNECTED,
    MQTT_SERVER_UNAVAILABLE,
    UNRECOVERABLE_CONNECT_FAIL,
    TLS_BAD_FINGERPRINT,
    TLS_NO_FINGERPRINT,
    TLS_NO_SSL,
    TLS_UNWANTED_FINGERPRINT,
    SUBSCRIBE_FAIL,
    INBOUND_QOS_ACK_FAIL,
    OUTBOUND_QOS_ACK_FAIL,
    INBOUND_PUB_TOO_BIG,
    OUTBOUND_PUB_TOO_BIG,
    BOGUS_PACKET,
    X_INVALID_LENGTH,
    NO_SERVER_DETAILS
};

    extern char*            getPktName(uint8_t type);

//#include"PANGO.h" // common namespace
//  "Pangolin" has what some (uninformed) folk might call "unorthodox" memory management. 
//  It contains a lot of code that LOOKS like it will leak memory. For those uninformed folk, 
//  (including the authors of other so-called MQTT libraries): "standard" techniques simply don't work 
//  with the current implementations of ESPAsyncTCP -> LwIP. 
//
//  For performance reasons, LwIP does not copy the data behind any pointer you send it. Since 
//  ESPAsyncTCP is little more than  a wrapper around LwIP, it simply "forwards" any pointer it
//  is given. Next you need to understand the TCP "nagle" algoritm but basically LwIP buffers
//  lots of small sends into a big packet that is worth sending (to reduce overheads), 
//  before "putting it on the wire" when *IT* decides and the caller has NO CONTROL over when
//  that happens.
//
//  THEREFORE:
//
//      WE must hold on to that pointer until LwIP has actually used it, which may be seconds later
//      and LONG after "standard" techniques have free'd it, incorrectly thinking that it is safe to
//      do so, since they have "fowarded it on". What they have in fact done is created a "dangling pointer".
//      
//      IF when LwIP DOES comes to use it the data it USED TO point to has "gone away" or been re-used 
//      (highly likely!) then pick your chosen failure mode from any of the following:
//       * packet data corruption
//       * mangled protocol control leading to closed connections (giving disconnect / reconnect loops) 
//       * the MOST fun - random exception crash / reboot loop. 
//
//      ALL of which will be painfully all-too-familar to users of other MQTT libraries
//
//  SOLUTION:
//
//  All packet data pointers most be retained until the packet they relate to has been ACKed by TCP
//  ===============================================================================================
//
//  ADFP A "delayed free" pointer to a malloc'd block that contains the whole actual packet data.
//  It will NOT get free'd until the TCP ack comes back for it
//  so anywhere one is used it will LOOK like a memory leak, but IT IS NOT!
//  
//  1:1 flow control of packets sent vs packets ACKed is enforced to avoid overlap
//  and ensure incoming TCP ACK matches the last packet sent
//  with its id in _uaId (acting together as a kind of singleton PANGO_PACKET_MAP)
// 
//  A "message block" wrapper for all "flying" ADFP pointers so we always know their length and have quick
//  access to important fields e.g. Qos, "remaining length" which would otherwise need to be derived or
//  recalculated every time
//
//  class mq = "message block": Basically a fancy struct: ALWAYS contained by copy, don't ever hold ptr to an mqm
//
//using PANGO_DELAYED_FREE   = uint8_t*;
//using ADFP                 = PANGO_DELAYED_FREE; // SOOO much less typing - PANGO "delayed free" pointer

#include<mbx.h>
#include<mqTraits.h>

using PANGO_FN_VOID        = std::function<void(void)>;
using PANGO_FN_U8PTR       = std::function<void(uint8_t*,uint8_t* base)>;
using PANGO_FN_U8PTRU8     = std::function<uint8_t*(uint8_t*)>;

using PANGO_PACKET_MAP      =std::map<uint16_t,mqttTraits>; // indexed by messageId
using PANGO_cbConnect       =std::function<void(bool)>;
using PANGO_cbDisconnect    =std::function<void(int8_t)>;
using PANGO_cbError         =std::function<void(uint8_t,uint32_t)>;
using PANGO_cbMessage       =std::function<void(const char* topic, const uint8_t* payload, size_t len,uint8_t qos,bool retain,bool dup)>;

class Packet;
class ConnectPacket;
class PublishPacket;

class PangolinMQTT: public AardvarkTCP {
        friend class Packet;
        friend class ConnectPacket;
        friend class PublishPacket;

               PANGO_cbConnect     _cbConnect=nullptr;
               PANGO_cbDisconnect  _cbDisconnect=nullptr;
               PANGO_cbError       _cbError=nullptr;
               PANGO_cbMessage     _cbMessage=nullptr;
               bool                _cleanSession=true;
               std::string         _clientId;
               bool                _connected=false;
               uint16_t            _keepalive=15 * PANGO_POLL_RATE;
               std::string         _password;
        static PANGO_PACKET_MAP    _inbound;
        static uint32_t            _nPollTicks;
        static uint32_t            _nSrvTicks;
        static PANGO_PACKET_MAP    _outbound;
               std::string         _username;
               std::string         _willPayload;
               uint8_t             _willQos;
               bool                _willRetain;
               std::string         _willTopic;
               
               void                _ACK(PANGO_PACKET_MAP* m,uint16_t id,bool inout); // inout true=INBOUND false=OUTBOUND
               void                _ACKoutbound(uint16_t id){ _ACK(&_outbound,id,false); }

               void                _cleanStart();
               void                _clearQQ(PANGO_PACKET_MAP* m);
               void                _destroyClient();
               void                _handlePacket(uint8_t* data, size_t len);
               void                _handlePublish(mqttTraits T);
        inline void                _hpDespatch(mqttTraits T);
        inline void                _hpDespatch(uint16_t id);
               void                _onDisconnect(int8_t r);
               void                _onPoll();
               void                _resendPartialTxns();
    public:
        PangolinMQTT();
                void               connect(const char* clientId="",bool session=true){ 
                        _cleanSession = session;
                        _clientId = clientId;
                        TCPconnect();
                }
                void               disconnect();
                const char*        getClientId(){ return _clientId.data(); }
                size_t inline      getMaxPayloadSize(){ return (_HAL_getFreeHeap() / 2) - PANGO_HEAP_SAFETY; }

                bool               mqttConnected(){ return _connected; }

                void               onMqttConnect(PANGO_cbConnect callback){ _cbConnect=callback; }
                void               onMqttDisconnect(PANGO_cbDisconnect callback){ _cbDisconnect=callback; }
                void               onMqttError(PANGO_cbError callback){ _cbError=callback; }
                void               onMqttMessage(PANGO_cbMessage callback){ _cbMessage=callback; }
                
                void               publish(const char* topic,const uint8_t* payload, size_t length, uint8_t qos=0,  bool retain=false);
                void               publish(const char* topic,const char* payload, size_t length, uint8_t qos=0,  bool retain=false);
                template<typename T>
                void publish(const char* topic,T v,const char* fmt="%d",uint8_t qos=0,bool retain=false){
                    char buf[16];
                    sprintf(buf,fmt,v);
                    publish(topic, reinterpret_cast<const uint8_t*>(buf), strlen(buf), qos, retain);
                }
//              Coalesce templates when C++17 available (if constexpr (x))
                void xPublish(const char* topic,const char* value, uint8_t qos=0,  bool retain=false) {
                    publish(topic,reinterpret_cast<const uint8_t*>(value),strlen(value),qos,retain);
                }
                void xPublish(const char* topic,String value, uint8_t qos=0,  bool retain=false) {
                    publish(topic,reinterpret_cast<const uint8_t*>(value.c_str()),value.length(),qos,retain);
                }
                void xPublish(const char* topic,std::string value, uint8_t qos=0,  bool retain=false) {
                    publish(topic,reinterpret_cast<const uint8_t*>(value.c_str()),value.size(),qos,retain);
                }
                template<typename T>
                void xPublish(const char* topic,T value, uint8_t qos=0,  bool retain=false) {
                    publish(topic,reinterpret_cast<uint8_t*>(&value),sizeof(T),qos,retain);
                }
                void xPayload(const uint8_t* payload,size_t len,char*& cp) {
                    char* p=reinterpret_cast<char*>(malloc(len+1));
                    memcpy(p,payload,len);
                    p[len]='\0';
                    cp=p;
                }
                void xPayload(const uint8_t* payload,size_t len,std::string& ss) {
                    char* cp;
                    xPayload(payload,len,cp);
                    ss.assign(cp,strlen(cp));
                    ::free(cp);
                }
                void xPayload(const uint8_t* payload,size_t len,String& duino) {
                    char* cp;
                    xPayload(payload,len,cp);
                    duino+=cp;
                    ::free(cp);
                }
                template<typename T>
                void xPayload(const uint8_t* payload,size_t len,T& value) {
                    if(len==sizeof(T)) memcpy(reinterpret_cast<T*>(&value),payload,sizeof(T));
                    else _notify(X_INVALID_LENGTH,len);
                }
                void               setKeepAlive(uint16_t keepAlive){ _keepalive = PANGO_POLL_RATE * keepAlive; }
                void               setServer(const char* url,const char* username="", const char* password = "",const uint8_t* fingerprint=nullptr);
                void               setWill(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr);
                void               subscribe(const char* topic, uint8_t qos);
                void               subscribe(std::initializer_list<const char*> topix, uint8_t qos);
                void               unsubscribe(const char* topic);
                void               unsubscribe(std::initializer_list<const char*> topix);
//
//              DO NOT CALL ANY FUNCTION STARTING WITH UNDERSCORE!!! _
//
                void               _notify(uint8_t e,int info=0);
#if PANGO_DEBUG
                void               dump(); // null if no debug
#endif
};

extern PangolinMQTT*        PANGOV3;