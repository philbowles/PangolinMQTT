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
#include "mqTraits.h"

PANGO_PACKET_MAP        PangolinMQTT::_inbound;
PANGO_PACKET_MAP        PangolinMQTT::_outbound;

#ifdef ARDUINO_ARCH_ESP32
    const char* _HAL_getUniqueId(){
        static char buf[19];
        sprintf(buf, "esp32-%12llX", ESP.getEfuseMac());
        return buf;
    }
#else
    const char*_HAL_getUniqueId(){
        static char buf[15];
        sprintf(buf, "esp8266-%06X", ESP.getChipId());
        return buf;
    }
#endif

PangolinMQTT*       PANGOV3;

uint32_t            PangolinMQTT::_nPollTicks;
uint32_t            PangolinMQTT::_nSrvTicks;

PangolinMQTT::PangolinMQTT(): AardvarkTCP(){
    PANGOV3=this;
    setNoDelay(true);

    onTCPconnect([=](){ if(!_connected) ConnectPacket cp{}; });
    onTCPdisconnect([=](int8 r){ PANGO_PRINT1("TCP CHOPPED US! %d\n",r); _onDisconnect(TCP_DISCONNECTED); });
    onTCPerror([=](int error,int info){ PANGO_PRINT1("TCP_ERROR %d info=%d\n",error,info); _onDisconnect(error); });
    onTCPpoll([=]{ _onPoll(); });

    rx([=](const uint8_t* data,size_t len){ _handlePacket((uint8_t*) data,len); });
}

void PangolinMQTT::setServer(const char* url,const char* username, const char* password,const uint8_t* fingerprint){
    _username = username;
    _password = password;
    TCPurl(url,fingerprint);
}

void PangolinMQTT::setWill(const char* topic, uint8_t qos, bool retain, const char* payload) {
  _willTopic = topic;
  _willQos = qos;
  _willRetain = retain;
  _willPayload = payload;
}

void PangolinMQTT::_ACK(PANGO_PACKET_MAP* m,uint16_t id,bool inout){ /// refakta?
    if(m->count(id)){
        ADFP data=((*m)[id]).data;
        Serial.printf("ACK %sBOUND id=0x%08x\n",inout ? "IN":"OUT",data);
        dumphex(data,((*m)[id]).len);
        mbx::clear(data);
        m->erase(id);
        dump();
    } else _notify(inout ? INBOUND_QOS_ACK_FAIL:OUTBOUND_QOS_ACK_FAIL,id); //PANGO_PRINT("WHO TF IS %d???\n",id);
}

void PangolinMQTT::_cleanStart(){
    PANGO_PRINT4("_cleanStart clrQQ inbound\n");
    _clearQQ(&_inbound);
    PANGO_PRINT4("_cleanStart clrQQ outbound\n");
    _clearQQ(&_outbound);
//  sanity checks
    if(mbx::pool.size()){
        Serial.printf("AAAAAAAAARGH! %d ORPHANED MBX!!!\n",mbx::pool.size());
        mbx::dump();
    }
    Packet::_nextId=1000; // SO much easier to differentiate client vs server IDs in Wireshark log :)
}

void PangolinMQTT::_clearQQ(PANGO_PACKET_MAP* m){
    for(auto &i:*m) mbx::clear(i.second.data);
    m->clear();
}

void PangolinMQTT::_hpDespatch(mqttTraits P){ if(_cbMessage) _cbMessage(P.topic.data(), P.payload, P.plen, P.qos, P.retain, P.dup); }

void PangolinMQTT::_hpDespatch(uint16_t id){ 
    Serial.printf("HPID\n");
    dump();
    _hpDespatch(_inbound[id]);
}

void PangolinMQTT::_handlePacket(uint8_t* data, size_t len){
    _nSrvTicks=0;
    mqttTraits traits(data,len);

    auto i=traits.start();
    uint16_t id=traits.id;

    switch (traits.type){
        case CONNACK:
            if(i[1]) _notify(UNRECOVERABLE_CONNECT_FAIL,i[1]);
            else {
                _connected=true;
                bool session=i[0] & 0x01;
                _resendPartialTxns();
                _nPollTicks=_nSrvTicks=0;
                PANGO_PRINT1("CONNECTED FH=%u MaxPL=%u SESSION %s\n",_HAL_getFreeHeap(),getMaxPayloadSize(),session ? "DIRTY":"CLEAN");
                if(_cbConnect) _cbConnect(session);
            }
            break;
        case PINGRESP:
        case UNSUBACK:
           break;
        case SUBACK:
            if(i[2] & 0x80) _notify(SUBSCRIBE_FAIL,id);
            break;
        case PUBACK:
        case PUBCOMP:
            _ACKoutbound(id);
            break;
        case PUBREC:
            {
                _outbound[id].pubrec=true;
                PubrelPacket prp(id);
            }
            break;
        case PUBREL:
            {
                if(_inbound.count(id)) {
                    _hpDespatch(_inbound[id]);
                    _ACK(&_inbound,id,true); // true = inbound
                } else PANGOV3->_notify(INBOUND_QOS_ACK_FAIL,id);
                PubcompPacket pcp(id); // pubrel
            }
            break;
       default:
            if(traits.isPublish()) _handlePublish(traits);
            else {
                _notify(BOGUS_PACKET,data[0]);
                PANGO_DUMP3(data,len);
            }
            break;
    }
}

void PangolinMQTT::_handlePublish(mqttTraits P){
    uint8_t qos=P.qos;
    uint16_t id=P.id;
    PANGO_PRINT4("_handlePublish %s id=%d @ QoS%d R=%s DUP=%d PL@%08X PLEN=%d\n",P.topic.data(),id,qos,P.retain ? "true":"false",P.dup,P.payload,P.plen);
    switch(qos){
        case 0:
            _hpDespatch(P);
            break;
        case 1:
            { 
                _hpDespatch(P);
                PubackPacket pap(id);
            }
            break;
        case 2:
        //  MQTT Spec. "method A"
            {
                PublishPacket pub(P.topic.data(),qos,P.retain,P.payload,P.plen,0,id); // build and HOLD until PUBREL force dup=0
                PubrecPacket pcp(id);
            }
            break;
    }
}

void PangolinMQTT::_notify(uint8_t e,int info){ 
    PANGO_PRINT1("NOTIFY e=%d inf=%d\n",e,info);
    if(_cbError) _cbError(e,info);
}

void PangolinMQTT::_onDisconnect(int8_t r) {
    #if PANGO_DEBUG > 0 
    PANGO_PRINT1("ON DISCONNECT FH=%u r=%d\n",_HAL_getFreeHeap(),r); 
    #endif
    _connected=false;
    if(_cbDisconnect) _cbDisconnect(r);
}

void PangolinMQTT::_onPoll() {
    if(_connected){
        ++_nPollTicks;
        ++_nSrvTicks;

        if(_nSrvTicks > ((_keepalive * 3) / 2)) { // safe headroom
            PANGO_PRINT1("T=%u SRV GONE? ka=%d Stix=%d\n",millis(),_keepalive,_nSrvTicks);
            _onDisconnect(MQTT_SERVER_UNAVAILABLE);
        }
        else {
            if(_nPollTicks > _keepalive){
//                _resendPartialTxns();
                PingPacket pp{};
                _nPollTicks=0;
            }
        }
    }
}

void PangolinMQTT::_resendPartialTxns(){
    std::vector<uint16_t> morituri;
    for(auto const& o:_outbound){
        mqttTraits m=o.second;
        if(--(m.retries)){
            if(m.pubrec){
                PANGO_PRINT4("WE ARE PUBREC'D ATTEMPT @ QOS2: SEND %d PUBREL\n",m.id);
                PubrelPacket prp(m.id);
            }
            else {
                PANGO_PRINT4("SET DUP & RESEND %d\n",m.id);
                m.data[0]|=0x08; // set dup & resend
                txdata(m.data,m.len,false);
            }
        }
        else {
            PANGO_PRINT4("NO JOY AFTER %d ATTEMPTS: QOS FAIL\n",PANGO_MAX_RETRIES);
            morituri.push_back(m.id); // all hope exhausted TODO: reconnect?
        }
    }
    for(auto const& i:morituri) _ACKoutbound(i);
}

//
//      PUBLIC
//
void PangolinMQTT::disconnect() {   
    PANGO_PRINT1("USER DCX\n");
    if(_connected) DisconnectPacket dp{};
    else _notify(TCP_DISCONNECTED);
}

void PangolinMQTT::publish(const char* topic, const uint8_t* payload, size_t length, uint8_t qos, bool retain) {
    if(_connected) PublishPacket pub(topic,qos,retain,payload,length,0,0);
    else _notify(TCP_DISCONNECTED);
}
// just because so many folk don't know the difference...sigh
void PangolinMQTT::publish(const char* topic, const char* payload, size_t length, uint8_t qos, bool retain) {
    publish(topic, reinterpret_cast<const uint8_t*>(payload), length, qos, retain);
}

void PangolinMQTT::subscribe(const char* topic, uint8_t qos) {
    if(_connected) SubscribePacket sub(topic,qos);
    else _notify(TCP_DISCONNECTED);
}

void PangolinMQTT::subscribe(std::initializer_list<const char*> topix, uint8_t qos) {
    if(_connected) SubscribePacket sub(topix,qos);
    else _notify(TCP_DISCONNECTED);
}

void PangolinMQTT::unsubscribe(const char* topic) {
    if(_connected) UnsubscribePacket usp(topic);
    else _notify(TCP_DISCONNECTED);
}

void PangolinMQTT::unsubscribe(std::initializer_list<const char*> topix) {
    if(_connected) UnsubscribePacket usp(topix);
    else _notify(TCP_DISCONNECTED);
}
//
//
//
#if PANGO_DEBUG
void PangolinMQTT::dump(){ 
    PANGO_PRINT4("DUMP ALL %d POOL BLOX (1st 64)\n",mbx::pool.size());
    for(auto & p:mbx::pool) {
        PANGO_PRINT4("%08X\n",p);
        PANGO_DUMP4(p,64);
    }

    PANGO_PRINT4("DUMP ALL %d PACKETS OUTBOUND\n",_outbound.size());
    for(auto & p:_outbound) p.second.dump();

    PANGO_PRINT4("DUMP ALL %d PACKETS INBOUND\n",_inbound.size());
    for(auto & p:_inbound) p.second.dump();

    PANGO_PRINT4("\n");
}
#endif