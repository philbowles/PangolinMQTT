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

#include"config.h"
#include<Arduino.h>
#include<functional>
#include<string>
#include<map>
#include <queue>

#ifdef ARDUINO_ARCH_ESP32
#include <AsyncTCP.h>
#elif defined(ARDUINO_ARCH_ESP8266)
#include <ESPAsyncTCP.h>
#elif defined(ARDUINO_ARCH_STM32)
#include <STM32AsyncTCP.h>
#else
#error Platform not supported
#endif

#if ASYNC_TCP_SSL_ENABLED
#include <tcp_axtls.h>
#define SHA1_SIZE 20
#endif

#define CSTR(x) x.c_str()
enum :uint8_t {
    CONNECT     = 0x10, // x
    CONNACK     = 0x20, // x
    PUBLISH     = 0x30, // x
    PUBACK      = 0x40, // x
    PUBREC      = 0x50, 
    PUBREL      = 0x62,
    PUBCOMP     = 0x70,
    SUBSCRIBE   = 0x82, // x
    SUBACK      = 0x90, // x
    UNSUBSCRIBE = 0xa2, // x
    UNSUBACK    = 0xb0, // x
    PINGREQ     = 0xc0, // x
    PINGRESP    = 0xd0, // x
    DISCONNECT  = 0xe0
};

enum PANGO_FAILURE : uint8_t {
    TCP_DISCONNECTED,
    MQTT_SERVER_UNAVAILABLE,
    UNRECOVERABLE_CONNECT_FAIL,
    TLS_BAD_FINGERPRINT,
    TCP_TIMEOUT,
    SUBSCRIBE_FAIL,
    INBOUND_QOS_FAIL,
    OUTBOUND_QOS_FAIL,
    INBOUND_QOS_ACK_FAIL,
    OUTBOUND_QOS_ACK_FAIL,
    INBOUND_PUB_TOO_BIG,
    OUTBOUND_PUB_TOO_BIG,
    BOGUS_PACKET,
    BOGUS_ACK
};

struct PANGO_PROPS {
  uint8_t qos;
  bool dup;
  bool retain;
};

#include"PANGO.h" // common namespace
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
//  class mb = "message block": Basically a fancy struct: ALWAYS contained by copy, don't ever hold ptr to an mb
//
using PANGO_DELAYED_FREE   = uint8_t*;
using ADFP                 = PANGO_DELAYED_FREE; // SOOO much less typing - PANGO "delayed free" pointer

using PANGO_BLOCK          = std::pair<size_t,uint8_t*>; // small sized ptr used internally by Packet class UPSCALE ANYWAY?
using PANGO_FN_VOID        = std::function<void(void)>;
using PANGO_FN_U8PTR       = std::function<void(uint8_t*,mb* base)>;
using PANGO_FN_U8PTRU8     = std::function<uint8_t*(uint8_t*)>;
using PANGO_BLOCK_Q        = std::queue<PANGO_BLOCK>;
using PANGO_PROPS_t        = struct PANGO_PROPS;

#include"mb.h"

using PANGO_PACKET_MAP      =std::map<uint16_t,mb>; // indexed by messageId
using PANGO_cbConnect       =std::function<void(bool)>;
using PANGO_cbDisconnect    =std::function<void(int8_t)>;
using PANGO_cbError         =std::function<void(uint8_t,int)>;
using PANGO_cbMessage       =std::function<void(const char*, uint8_t*, PANGO_PROPS_t , size_t, size_t, size_t)>;
using PANGO_cbPublish       =std::function<void(uint16_t packetId)>;
using PANGO_cbSubscribe     =std::function<void(uint16_t, uint8_t)>;
using PANGO_cbUnsubscribe   =std::function<void(uint16_t)>;

class Packet;
class ConnectPacket;
class PublishPacket;

class PangolinMQTT {
        friend class Packet;
        friend class ConnectPacket;
        friend class PublishPacket;
        
               PANGO_cbConnect      _cbConnect=nullptr;
               PANGO_cbDisconnect   _cbDisconnect=nullptr;
        static PANGO_cbError        _cbError;
               PANGO_cbMessage      _cbMessage=nullptr;
               PANGO_cbPublish      _cbPublish=nullptr;
               PANGO_cbSubscribe    _cbSubscribe=nullptr;
               PANGO_cbUnsubscribe  _cbUnsubscribe=nullptr;
#if ASYNC_TCP_SSL_ENABLED
               uint8_t             _fingerprint[SHA1_SIZE];
#endif
        static bool                _cleanSession;
               std::string         _clientId;
               std::string         _host;
               IPAddress           _ip;
        static uint16_t            _keepalive;
        static std::string         _password;
               uint16_t            _port;
               bool                _useIp;
        static std::string         _username;
        static std::string         _willPayload;
        static uint8_t             _willQos;
        static bool                _willRetain;
        static std::string         _willTopic;

               void                _cleanStart();
               void                _destroyClient();
               void                _handlePublish(mb);
        inline void                _hpDespatch(mb);
               void                _onData(uint8_t* data, size_t len);
               void                _onDisconnect(int8_t r);
               void                _onPoll(AsyncClient* client);
               uint8_t*            _packetReassembler(mb);

#ifdef PANGO_DEBUG
                void                debugHandler(uint8_t* payload,size_t length);
#endif
    public:
        PangolinMQTT();
                void                connect();
                bool                connected(){ return PANGO::TCP; };
                void                disconnect(bool force = false);
                const char*         getClientId(){ return _clientId.c_str(); }
                size_t inline       getMaxPayloadSize(){ return (PANGO::_HAL_getFreeHeap() / 2) - PANGO_HEAP_SAFETY; }
                void                onConnect(PANGO_cbConnect callback){ _cbConnect=callback; }
                void                onDisconnect(PANGO_cbDisconnect callback){ _cbDisconnect=callback; }
                void                onError(PANGO_cbError callback){ _cbError=callback; }
                void                onMessage(PANGO_cbMessage callback){ _cbMessage=callback; }
                void                onPublish(PANGO_cbPublish callback){ _cbPublish=callback; }
                void                onSubscribe(PANGO_cbSubscribe callback){ _cbSubscribe=callback; }
                void                onUnsubscribe(PANGO_cbUnsubscribe callback){ _cbUnsubscribe=callback; }
                uint16_t            publish(const char* topic, uint8_t qos, bool retain, uint8_t* payload, size_t length, bool dup); // <- stupid!!!
                void                publish(const char* topic, uint8_t qos, bool retain, std::string payload){ publish(topic,qos,retain, (uint8_t*) payload.data(), (size_t) payload.size()+1,false); }
                void                publish(const char* topic, uint8_t qos, bool retain, String payload){ publish(topic,qos,retain, (uint8_t*) payload.c_str(), (size_t) payload.length()+1,false); }
                void                setCleanSession(bool cleanSession){ _cleanSession = cleanSession; }
                void                setClientId(const char* clientId){ _clientId = clientId; }
                void                setCredentials(const char* username, const char* password = nullptr);
                void                setKeepAlive(uint16_t keepAlive){ _keepalive = PANGO_POLL_RATE * keepAlive; }
                void                setServer(IPAddress ip, uint16_t port);
                void                setServer(const char* host, uint16_t port);
                void                setWill(const char* topic, uint8_t qos, bool retain, const char* payload = nullptr);
                uint16_t            subscribe(const char* topic, uint8_t qos);
                uint16_t            unsubscribe(const char* topic);
//
//
//
#if ASYNC_TCP_SSL_ENABLED
//                void                setSecure(bool secure){ _secure=secure; }
                void                serverFingerprint(const uint8_t* fingerprint);
#endif

//
//              DO NOT CALL ANY FUNCTION STARTING WITH UNDERSCORE!!! _
//
                void                _handlePacket(mb);
                void                _fatal(uint8_t e,int info=0);
                void                _notify(uint8_t e,int info=0);
};