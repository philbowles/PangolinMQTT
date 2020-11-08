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
#include <PangolinMQTT.h>
#include "Packet.h"

std::string          PangolinMQTT::_username;
std::string          PangolinMQTT::_password;
std::string          PangolinMQTT::_willTopic;
std::string          PangolinMQTT::_willPayload;
uint8_t              PangolinMQTT::_willQos;
bool                 PangolinMQTT::_willRetain;
uint16_t             PangolinMQTT::_keepalive=15 * PANGO_POLL_RATE; // 10 seconds
bool                 PangolinMQTT::_cleanSession=true;
PANGO_cbError        PangolinMQTT::_cbError=nullptr;

PangolinMQTT::PangolinMQTT() { PANGO::LIN=this; }

void PangolinMQTT::setCredentials(const char* username, const char* password) {
  _username = username;
  _password = password;
}

void PangolinMQTT::setWill(const char* topic, uint8_t qos, bool retain, const char* payload) {
  _willTopic = topic;
  _willQos = qos;
  _willRetain = retain;
  _willPayload = payload;
}

void PangolinMQTT::setServer(IPAddress ip, uint16_t port) {
  _useIp = true;
  _ip = ip;
  _port = port;
}

void PangolinMQTT::setServer(const char* host, uint16_t port) {
  _useIp = false;
  _host = host;
  _port = port;
}
//
#if ASYNC_TCP_SSL_ENABLED
void PangolinMQTT::serverFingerprint(const uint8_t* fingerprint) {
    memcpy(_fingerprint, fingerprint, SHA1_SIZE);
    PANGO::_secure=true;
}
#endif
void PangolinMQTT::_destroyClient(){
    if(PANGO::TCP) {
        PANGO::TCP->onDisconnect([this](void* obj, AsyncClient* c) { }); // prevent recursion
        delete PANGO::TCP; // causes a recurse!
        PANGO::TCP=nullptr;
    }
}

void PangolinMQTT::_hpDespatch(mb m){
//    Serial.printf("_hpDespatch %s id=%d @ QoS%d R=%s DUP=%d PL@%08X PLEN=%d\n",m.topic.c_str(),m.id,m.qos,m.retain ? "true":"false",m.dup,m.payload,m.plen);
    if(_cbMessage) _cbMessage(m.topic.c_str(), m.payload, PANGO_PROPS_t {m.qos,m.dup,m.retain}, m.plen, 0, m.plen);
}

void PangolinMQTT::_onDisconnect(int8_t r) {
    PANGO_PRINT("DISCONNECT FH=%u r=%d\n",PANGO::_HAL_getFreeHeap(),r);
    PANGO::_clearQ(&PANGO::TXQ);
    PANGO::_clearFragments();
    PANGO::_resetPingTimers();
    if(PANGO::TCP){
        _destroyClient();
        if(_cbDisconnect) _cbDisconnect(r);
    }
}

void PangolinMQTT::_notify(uint8_t e,int info){ 
    PANGO_PRINT("NOTIFY e=%d inf=%d\n",e,info);
    if(_cbError) _cbError(e,info);
}

void PangolinMQTT::_fatal(uint8_t e,int info){
    if(_cbError) _cbError(e,info);
    PANGO::TCP->close(true);
    Serial.printf("FATAL ERROR %d INFO=%d - POWER CYCLE REQUIRED\n",e,info);
    while(1) PANGO::_HAL_feedWatchdog();
}

void PangolinMQTT::_handlePublish(mb m){
    PANGO_PRINT("_handlePublish %s id=%d @ QoS%d R=%s DUP=%d PL@%08X PLEN=%d\n",m.topic.c_str(),m.id,m.qos,m.retain ? "true":"false",m.dup,m.payload,m.plen);
    switch(m.qos){
        case 0:
            _hpDespatch(m);
            break;
        case 1:
            { 
                _hpDespatch(m);
                PubackPacket pap(m.id);
            }
            break;
        case 2:
        //  MQTT Spec. "method A"
            {
                PublishPacket pub(m.topic.c_str(),m.qos,m.retain,m.payload,m.plen,0,m.id); // build and HOLD until PUBREL force dup=0
                PubrecPacket pcp(m.id);
            }
            break;
    }
}

void PangolinMQTT::_handlePacket(mb m){
    uint8_t*    i=m.start();
    uint16_t    id=PANGO::_peek16(i);
//    PANGO_PRINT("_handlePacket %s %08x len=%d id=%d, i=%08X\n",PANGO::getPktName(m.data[0]),m.data,m.len,id,i);
    switch (m.data[0]){
        case CONNACK:
            if(i[1]) _fatal(UNRECOVERABLE_CONNECT_FAIL,i[1]);
            else {
                PANGO::_space=PANGO::TCP->space();
                bool session=i[0] & 0x01;
                //Serial.printf("\nSESSION IS %s\n",session ? "DIRTY":"CLEAN");
                Packet::_resendPartialTxns();
                PANGO_PRINT("CONNECTED FH=%u SH=%u\n",PANGO::_HAL_getFreeHeap(),getMaxPayloadSize());
                if(_cbConnect) _cbConnect(session);
            }
        case PINGRESP:
            break;
        case SUBACK:
            if(i[2] & 0x80) _fatal(SUBSCRIBE_FAIL,id);
            else if(_cbSubscribe) _cbSubscribe(id,i[2]);
            break;
        case UNSUBACK:
            if(_cbUnsubscribe) _cbUnsubscribe(id);
            break;
        case PUBACK:
            Packet::ACKoutbound(id);
            if(_cbPublish) _cbPublish(id);
            break;
        case PUBREC:
            {
                Packet::_outbound[id].pubrec=true;
                PubrelPacket prp(id);
            }
            break;
        case PUBREL:
            {
                if(Packet::_inbound.count(id)) {
                    _hpDespatch(Packet::_inbound[id]);
                    Packet::_ACK(&Packet::_inbound,id,true); // true = inbound
                } else PANGO::LIN->_notify(INBOUND_QOS_ACK_FAIL,id);
                PubcompPacket pcp(id); // pubrel
            }
            break;
        case PUBCOMP:
            Packet::ACKoutbound(id);
            if(_cbPublish) _cbPublish(id);
            break;
        default:
            if(m.isPub()) _handlePublish(m);
            else {
                _notify(BOGUS_PACKET,m.data[0]);
                PANGO::dumphex(m.data,m.len);
            }
            break;
    }
    m.clear();
} 
//
// packet chopper / despatcher
//
uint8_t* PangolinMQTT::_packetReassembler(mb m){
    static uint32_t expecting=0;
    static uint32_t received=0;
    static bool midFrag=false;
    static bool discard=false;
    uint32_t    me=m.len; // unless told otherwise;
    if(midFrag){
        if(expecting-received>m.len){
            if(!discard) PANGO::_saveFragment(m);
            received+=m.len;
//            PANGO_PRINT("MIDFRAG: expecting %d received %d remaining=%d\n",expecting,received,expecting-received);
        }
        else {
            midFrag=false;
            if(!discard){
                uint8_t* bpp=static_cast<uint8_t*>(malloc(expecting)); // NOT a leak!!
//                PANGO_PRINT("REBUILD %08X len=%d\n",bpp,expecting);
                size_t running=0;
                for(auto const& f:PANGO::_fragments){
//                    PANGO_PRINT("ASSEMBLE from %08X len=%d frag=%d running=%d\n",(void*) f.data,f.len,f.frag,running);
                    memcpy(bpp+running,f.data,f.len);
                    running+=f.len;
                }
                PANGO::_clearFragments();
                uint32_t charlie=expecting-received;
//                PANGO_PRINT("TAIL END CHARLIE IS %d E=%d R=%d\n",charlie,expecting,received);
                memcpy(bpp+expecting-charlie,m.data,charlie);
                _handlePacket(mb(bpp,true)); // frees bpp
                if(charlie<m.len) {
//                    PANGO_PRINT("E-R < m.len RETURN ME=%d\n",charlie);
                    me=charlie;
                } //else PANGO_PRINT("DEFAULT ME=%d\n",me);
            } 
            else {
//                PANGO_PRINT("DODGED A BULLET!\n");
                _notify(INBOUND_PUB_TOO_BIG,expecting);
                discard=false;
            }
        }
    }
    else {
        mb tmp(m.data,false);
//        PANGO_PRINT("NON-REBUILD %08X len=%d\n",tmp.data,tmp.len);
        if(tmp.len<getMaxPayloadSize()){
            if(tmp.len > m.len){
                expecting=tmp.len;
                received=m.len;
//                PANGO_PRINT("START ASSEMBLY: expecting %d received %d remaining=%d\n",expecting,received,expecting-received);
                PANGO::_saveFragment(m);
                midFrag=true;
            }
            else {
                me=tmp.len;
                _handlePacket(tmp);
            }
        } else {
            PANGO_PRINT("TOO HOT TO HANDLE %u > %u\n",tmp.len,getMaxPayloadSize());
            expecting=tmp.len;
            received=m.len;
            discard=true;
            midFrag=true;
        }
    }
    return m.data+me;
}

void PangolinMQTT::_onData(uint8_t* data, size_t len) {
    uint8_t*    p=data;
    uint8_t     offset=0;
    size_t      N=0;
    PANGO_PRINT("<---- FROM WIRE %s %08X len=%d\n",PANGO::getPktName(data[0]),data,len);
//    PANGO::dumphex(data,len);
    PANGO::_resetPingTimers();
    do {
        p=_packetReassembler(mb(data+len-p,p));
        PANGO::_HAL_feedWatchdog();
    } while (p < data + len);
}

void PangolinMQTT::_onPoll(AsyncClient* client) {
    if(PANGO::TCP){
        ++PANGO::_nPollTicks;
        ++PANGO::_nSrvTicks;

        if(PANGO::_nSrvTicks > ((_keepalive * 3) / 2)) { // safe headroom
            PANGO_PRINT("T=%u SRV GONE? ka=%d tix=%d\n",millis(),_keepalive,PANGO::_nSrvTicks);
            _onDisconnect(MQTT_SERVER_UNAVAILABLE);
        }
        else {
            if(PANGO::_nPollTicks > _keepalive){
                Packet::_resendPartialTxns();
                if(!(PANGO::TXQ.size())) PingPacket pp{}; 
            }
        }
    }
}

void PangolinMQTT::connect() {
    if(PANGO::TCP) return;
    if(_clientId=="") _clientId=PANGO::_HAL_getUniqueId();
    PANGO_PRINT("CONNECT as %s FH=%u session=%d\n",_clientId.c_str(),PANGO::_HAL_getFreeHeap(),_cleanSession);
    if(_cleanSession) _cleanStart();
    PANGO::TCP=new AsyncClient;
    PANGO::TCP->setNoDelay(true);
    PANGO::TCP->onConnect([this](void* obj, AsyncClient* c) { 
#if ASYNC_TCP_SSL_ENABLED
    if(PANGO::_secure) {
        SSL* clientSsl = PANGO::TCP->getSSL();
        if (ssl_match_fingerprint(clientSsl, _fingerprint) != SSL_OK) {
            _fatal(TLS_BAD_FINGERPRINT);
            return;
        }
    }
#endif
        ConnectPacket cp{};
    }); // *NOT* A MEMORY LEAK! :)
    PANGO::TCP->onDisconnect([this](void* obj, AsyncClient* c) { PANGO_PRINT("TCP CHOPPED US!\n"); _onDisconnect(TCP_DISCONNECTED); });
    PANGO::TCP->onError([this](void* obj, AsyncClient* c,int error) { PANGO_PRINT("TCP_ERROR %d\n",error); _onDisconnect(error); });
    PANGO::TCP->onAck([this](void* obj, AsyncClient* c,size_t len, uint32_t time){ PANGO::_ackTCP(len,time); }); 
    PANGO::TCP->onData([this](void* obj, AsyncClient* c, void* data, size_t len) { _onData(static_cast<uint8_t*>(data), len); });
    PANGO::TCP->onPoll([this](void* obj, AsyncClient* c) { _onPoll(c); });
// tidy this + whole _useIP bollocks
#if ASYNC_TCP_SSL_ENABLED
  if (_useIp) {
    PANGO::TCP->connect(_ip, _port, PANGO::_secure);
  } else {
   PANGO::TCP->connect(_host.c_str(), _port, PANGO::_secure);
  }
#else
  if (_useIp) {
   PANGO::TCP->connect(_ip, _port);
  } else {
    PANGO::TCP->connect(_host.c_str(), _port);
  }
#endif

}

void PangolinMQTT::disconnect(bool force) {
    PANGO_PRINT("USER DCX\n");
    if (!PANGO::TCP) return;
    DisconnectPacket dp{};
}

uint16_t PangolinMQTT::subscribe(const char* topic, uint8_t qos) {
    if (!PANGO::TCP) return 0;
    SubscribePacket sub(topic,qos);
    return sub._id;
}

uint16_t PangolinMQTT::unsubscribe(const char* topic) {
    if (!PANGO::TCP) return 0;
    UnsubscribePacket usp(topic);
    return usp._id;
}

uint16_t PangolinMQTT::publish(const char* topic, uint8_t qos, bool retain, uint8_t* payload, size_t length, bool dup) {
    if (!PANGO::TCP) return 0;
    PublishPacket pub(topic,qos,retain,payload,length,dup,0);
    return pub._id; // 'kin pointless
}

void PangolinMQTT::_cleanStart(){
    for(auto &i:Packet::_inbound) i.second.clear();
    Packet::_inbound.clear();

    for(auto &i:Packet::_outbound) i.second.clear();
    Packet::_outbound.clear();

    PANGO_PRINT("We are now clean:)\n");
    Packet::_nextId=1000; // SO much easier to differentiate client vs server IDs in Wireshark log :)
}
