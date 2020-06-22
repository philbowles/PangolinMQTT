#include <PangolinMQTT.h>
#include <Packet.h>
//#include <unordered_set>
#ifdef PANGO_DEBUG
    bool IACK=false;
    bool OACK=false;
    bool IREC=false;
    bool IREL=false;
    bool ICOM=false;
    bool OREC=false;
    bool OREL=false;
    bool OCOM=false;
#endif

std::string          PangolinMQTT::_username;
std::string          PangolinMQTT::_password;
std::string          PangolinMQTT::_willTopic;
std::string          PangolinMQTT::_willPayload;
uint8_t              PangolinMQTT::_willQos;
bool                 PangolinMQTT::_willRetain;
uint16_t             PangolinMQTT::_keepalive=15 * PANGO_POLL_RATE; // 10 seconds
bool                 PangolinMQTT::_cleanSession=true;
std::string          PangolinMQTT::_clientId;
PANGO_cbError        PangolinMQTT::_cbError=nullptr;

void PangolinMQTT::_destroyClient(){
    if(PANGO::TCP) {
        PANGO::TCP->onDisconnect([this](void* obj, AsyncClient* c) { }); // prevent recursion
        delete PANGO::TCP; // causes a recurse!
        PANGO::TCP=nullptr;
    }
}

PangolinMQTT::PangolinMQTT() {
    PANGO::LIN=this;
#ifdef ARDUINO_ARCH_ESP32
  sprintf(_generatedClientId, "esp32-%06llx", ESP.getEfuseMac());
#elif defined(ARDUINO_ARCH_ESP8266)
  sprintf(_generatedClientId, "esp8266-%06x", ESP.getChipId());
#endif
  _clientId = _generatedClientId;
}

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

void PangolinMQTT::_onDisconnect(int8_t r) {
    PANGO_PRINT("DISCONNECT FH=%u r=%d\n",ESP.getFreeHeap(),r);
    PANGO::_clearQ(&PANGO::RXQ);
    PANGO::_clearQ(&PANGO::TXQ);
    PANGO::_clearFragments();
    PANGO::_resetPingTimers();
    if(PANGO::TCP){
        _destroyClient();
        if(_cbDisconnect) _cbDisconnect(r);
    }
}

void PangolinMQTT::_hpDespatch(mb m){
    if(_cbMessage) _cbMessage(m.topic.c_str(), m.payload, PANGO_PROPS_t {m.qos,m.dup,m.retain}, m.plen, 0, m.plen);
}

void PangolinMQTT::_notify(uint8_t e,int info){ 
    PANGO_PRINT("NOTIFY e=%d inf=%d\n",e,info);
    if(_cbError) _cbError(e,info);
}

void PangolinMQTT::_handlePublish(mb m){
    PANGO_PRINT("INBOUND PUBLISH %s id=%d @ QoS%d R=%s DUP=%d PL@%08X PLEN=%d\n",m.topic.c_str(),m.id,m.qos,m.retain ? "true":"false",m.dup,m.payload,m.plen);

#ifdef PANGO_DEBUG
    if(m.topic=="PANGO"){
        PANGO_PRINT("PANGO DEBUG TOPIC FH=%u\n",ESP.getFreeHeap());
        debugHandler(m.payload,m.plen);
        return;
    }
#endif
    switch(m.qos){
        case 1:
            {
#ifdef PANGO_DEBUG
            if(OACK) PANGO_PRINT("BROKEN OUTBOUND PUBACK FOR QOS1 SESSION RECOVERY TESTING\n");
            else {
#endif
                PubackPacket pap(m.id);
#ifdef PANGO_DEBUG
            }
#endif
            }
        case 0:
            _hpDespatch(m);
            break;
        case 2:
        //  MQTT Spec. "method A"
            {
                PubrecPacket pcp(m.id); // pubrel
                PublishPacket pub(m.topic.c_str(),m.qos,m.retain,m.payload,m.plen,m.dup,m.id); // build and HOLD until PUBREL
            }
            break;
    }
}

void PangolinMQTT::_handlePacket(mb m){
    PANGO_PRINT("_handlePacket %s %08x len=%d\n",PANGO::getPktName(m.data[0]),m.data,m.len);
    uint8_t*    i=m.start();
    uint16_t    id=PANGO::_peek16(i);
    switch (m.data[0]){
        case CONNACK:
            if(i[1]) _onDisconnect(i[1]);
            else {
                PANGO::_space=PANGO::TCP->space();
                bool session=i[0] & 0x01;
                if(!session) _cleanStart();
                else {
                    PANGO_PRINT("ATTEMPT SESSION RECOVERY\n");
                    Packet::_clearPacketMap();
                }
                PANGO_PRINT("CONNECTED FH=%u SH=%u\n",ESP.getFreeHeap(),getMaxPayloadSize());
#ifdef PANGO_DEBUG
//              our own topic for debugging
                PANGO_PRINT("Auto-subscribed to PANGO\n");
                SubscribePacket pango("PANGO",0);
#endif                    
                if(_cbConnect) _cbConnect(session);
            }
        case PINGRESP:
            break;
        case SUBACK:
            if(i[3] & 0x80) _notify(SUBSCRIBE_FAIL,id);
            else if(_cbSubscribe) _cbSubscribe(id,i[2]);
            break;
        case UNSUBACK:
            if(_cbUnsubscribe) _cbUnsubscribe(id);
            break;
        case PUBACK:
#ifdef PANGO_DEBUG
            if(IACK) PANGO_PRINT("BROKEN INBOUND PUBACK FOR QOS1 SESSION RECOVERY TESTING\n");
            else {
#endif
            Packet::ACKoutbound(id);
            if(_cbPublish) _cbPublish(id);
#ifdef PANGO_DEBUG
            }
#endif
            if(_cbPublish) _cbPublish(id);
            break;
        case PUBREC:
#ifdef PANGO_DEBUG
            if(IREC) PANGO_PRINT("BROKEN INBOUND PUBREC FOR QOS2 SESSION RECOVERY TESTING\n");
            else {
#endif
            Packet::_outbound[id].pubrec=true;
            { PubrelPacket prp(id); }//Serial.printf("T=%u PUBREL %d\n",millis(),id); }
#ifdef PANGO_DEBUG
            }
#endif
            break;
        case PUBREL:
#ifdef PANGO_DEBUG
            if(IREL) PANGO_PRINT("BROKEN INBOUND PUBREL FOR QOS2 SESSION RECOVERY TESTING\n");
            else {
#endif
            {
                if(Packet::_inbound.count(id)) {
                    _hpDespatch(Packet::_inbound[id]);
                    Packet::_ACK(&Packet::_inbound,id,true); // true = inbound
                    PubcompPacket pcp(id); // pubrel
                } else _notify(INBOUND_QOS_FAIL,id); // id is pointless, but: what the hell?
            }
#ifdef PANGO_DEBUG
            }
#endif
            break;
        case PUBCOMP:
#ifdef PANGO_DEBUG
            if(ICOM) PANGO_PRINT("BROKEN INBOUND PUBCOMP FOR QOS2 SESSION RECOVERY TESTING\n");
            else {
#endif
            Packet::ACKoutbound(id);
            if(_cbPublish) _cbPublish(id);
#ifdef PANGO_DEBUG
            }
#endif
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
//    m.dump();
    uint32_t    me=m.len; // unless told otherwise;
    if(midFrag){
        if(expecting-received>m.len){
            if(!discard) PANGO::_saveFragment(m);
            received+=m.len;
            PANGO_PRINT("MIDFRAG: expecting %d received %d remaining=%d\n",expecting,received,expecting-received);
        }
        else {
            midFrag=false;
            if(!discard){
                uint8_t* bpp=static_cast<uint8_t*>(malloc(expecting)); // NOT a leak!!
                PANGO_PRINT("REBUILD %08X len=%d\n",bpp,expecting);
                size_t running=0;
                for(auto const& f:PANGO::_fragments){
                    PANGO_PRINT("ASSEMBLE from %08X len=%d frag=%d running=%d\n",(void*) f.data,f.len,f.frag,running);
                    memcpy(bpp+running,f.data,f.len);
                    running+=f.len;
                }
                PANGO::_clearFragments();
                memcpy(bpp+expecting-m.len,m.data,m.len);
                _handlePacket(mb(bpp,true)); // frees bpp
                if(expecting-received<m.len) me=expecting-received;
            } 
            else {
                PANGO_PRINT("DODGED A BULLET!\n");
                _notify(INBOUND_PUB_TOO_BIG,expecting);
                discard=false;
            }
        }
    }
    else {
        mb tmp(m.data,false);
        if(tmp.len<getMaxPayloadSize()){
            if(tmp.len > m.len){
                expecting=tmp.len;
                received=m.len;
                PANGO_PRINT("START ASSEMBLY: expecting %d received %d remaining=%d\n",expecting,received,expecting-received);
                PANGO::_saveFragment(m);
                midFrag=true;
            }
            else {
                me=m.len;
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
    PANGO::_resetPingTimers();
    do {
        p=_packetReassembler(mb(data+len-p,p));
        ESP.wdtFeed(); // move to fragment handler?
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
                Packet::_clearPacketMap();
                bool noPing=PANGO::TXQ.size();
                if(!noPing) PingPacket pp{}; 
            }
        }
    }
}

void PangolinMQTT::connect() {
    if (PANGO::TCP) return;
    PANGO_PRINT("CONNECT FH=%u\n",ESP.getFreeHeap());

    PANGO::TCP=new AsyncClient;
    PANGO::TCP->setNoDelay(true);
    PANGO::TCP->onConnect([this](void* obj, AsyncClient* c) { ConnectPacket cp{}; }); // *NOT* A MEMORY LEAK! :)
    PANGO::TCP->onDisconnect([this](void* obj, AsyncClient* c) { PANGO_PRINT("TCP CHOPPED US!\n"); _onDisconnect(TCP_DISCONNECTED); });
    PANGO::TCP->onError([this](void* obj, AsyncClient* c,int error) { PANGO_PRINT("TCP_ERROR %d\n",error); _onDisconnect(error); });
    PANGO::TCP->onAck([this](void* obj, AsyncClient* c,size_t len, uint32_t time){ PANGO::_ackTCP(len,time); }); 
    PANGO::TCP->onData([this](void* obj, AsyncClient* c, void* data, size_t len) { _onData(static_cast<uint8_t*>(data), len); });
    PANGO::TCP->onPoll([this](void* obj, AsyncClient* c) { _onPoll(c); });

    if (_useIp) PANGO::TCP->connect(_ip, _port);
    else PANGO::TCP->connect(CSTR(_host), _port);
}

void PangolinMQTT::disconnect(bool force) {
    if (!PANGO::TCP) return;
//    PANGO::TCP->close(force);
    PANGO_PRINT("USER DCX\n");
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

    PANGO_PRINT("We are now next to godly :)\n");
    Packet::_nextId=1000;
}
//
//   DEBUG TOPIC HANDLER
//
#ifdef PANGO_DEBUG
void PangolinMQTT::debugHandler(uint8_t* payload,size_t length){
    PANGO_PRINT("PANGO DEBUG HANDLER\n");
    PANGO::dumphex(payload,length);
    char sp[length+1];
    memcpy(sp,payload, length);
    sp[length]='\0';
    std::string pl(sp);
    if(pl=="dump"){ PANGO::dump(); }
    else if(pl=="dirty"){ 
        _onDisconnect(97);
        // force dirty
    }
    else if(pl=="clean"){
        _onDisconnect(97);
        // force dirty

    }
    else if(pl=="disco"){ _onDisconnect(99); }
    else if(pl=="reboot"){ ESP.restart(); }
    else if(pl=="fix"){ 
        IACK=false;
        OACK=false;
        IREC=false;
        IREL=false;
        ICOM=false;
        OREL=false;
        OREC=false;
        OCOM=false;
        Serial.printf("\n\nFIX\n\n");
    }
    else if(pl=="IACK"){ IACK=true; Serial.printf("\n\nIACK\n\n"); }
    else if(pl=="OACK"){ OACK=true; Serial.printf("\n\nOACK\n\n"); }
    else if(pl=="IREC"){ IREC=true; Serial.printf("\n\nIREC\n\n"); }
    else if(pl=="IREL"){ IREL=true; Serial.printf("\n\nIREL\n\n"); }
    else if(pl=="ICOM"){ ICOM=true; Serial.printf("\n\nICOM\n\n"); }
    else if(pl=="OREC"){ OREC=true; Serial.printf("\n\nOREC\n\n"); }
    else if(pl=="OREL"){ OREL=true; Serial.printf("\n\nOREL\n\n"); }
    else if(pl=="OCOM"){ OCOM=true; Serial.printf("\n\nOCOM\n\n"); }
    else PANGO_PRINT("UNKNOWN CMD %s\n",pl.c_str());
}
#endif