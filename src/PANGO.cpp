#include<PangolinMQTT.h>
#include<Packet.h> // remove this by moving packet statics into asmq

namespace PANGO {
            AsyncClient*        TCP;
            PANGO_MSG_Q         TXQ;
            PANGO_MSG_Q         RXQ;
            PangolinMQTT*       LIN;
            PANGO_FRAGMENTS     _fragments={};
            uint16_t            _maxRetries=PANGO_MAX_RETRIES;
            uint32_t            _nPollTicks;
            uint32_t            _nSrvTicks;
            size_t              _space=536; // rogue starting value as true value not known untill after connect!

            void                _ackTCP(size_t len, uint32_t time);
            void                _clearFragments();
            void                _clearQ(PANGO_MSG_Q*);
            PANGO_REM_LENGTH    _getRemainingLength(uint8_t* p);
            uint16_t            _peek16(uint8_t* p){ return (*(p+1))|(*p << 8); }
            void                _release(mb);
            void                _resetPingTimers(){ _nPollTicks=_nSrvTicks=0; }
            void                _runRXQ();
            void                _runTXQ();
            void                _saveFragment(mb);
            void                _send(mb);
            void                _txPacket(mb);
            void                _rxPacket(mb);
//
            void                dump(); // null if no PANGO_DEBUG
            void                dumphex(uint8_t* mem, size_t len,uint8_t W);
            char*               payloadToCstring(uint8_t* data,size_t len);
            int                 payloadToInt(uint8_t* data,size_t len);
            std::string         payloadToStdstring(uint8_t* data,size_t len);
            String              payloadToString(uint8_t* data,size_t len);
#ifdef PANGO_DEBUG
            std::map<uint8_t,char*> pktnames={
                {0x10,"CONNECT"},
                {0x20,"CONNACK"},
                {0x30,"PUBLISH"},
                {0x40,"PUBACK"},
                {0x50,"PUBREC"},
                {0x60,"PUBREL"},
                {0x70,"PUBCOMP"},
                {0x80,"SUBSCRIBE"},
                {0x90,"SUBACK"},
                {0xa0,"UNSUBSCRIBE"},
                {0xb0,"UNSUBACK"},
                {0xc0,"PINGREQ"},
                {0xd0,"PINGRESP"},
                {0xe0,"DISCONNECT"}
            };
            char*         getPktName(uint8_t type){
                uint8_t t=type&0xf0;
                if(pktnames.count(t)) return pktnames[t];
                else{
                    static char buf[3];
                    sprintf(buf,"%02X",type);
                    return buf;
                }
            }
#endif
}

void PANGO::_ackTCP(size_t len, uint32_t time){
//    PANGO_PRINT("TCP ACK LENGTH=%d\n",len);
    size_t amtToAck=len;
    while(amtToAck){
        if(!TXQ.empty()){
            mb tmp=TXQ.front();
            TXQ.pop();
            amtToAck-=tmp.len;
//            PANGO_PRINT("****************** ACKACKAMT=%d ACK %s LENGTH=%d remaining=%d\n",amtToAck,PANGO::getPktName(tmp.data[0]),tmp.len,amtToAck);
            tmp.ack();
        }
        else {
//            PANGO_PRINT("WAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAH remaining=%d\n",amtToAck);
            PANGO::LIN->_notify(BOGUS_ACK,len);
            break;
        }
    }
    _runTXQ();
}

void PANGO::_clearFragments(){
//    PANGO_PRINT("CLEAR %d FRAGMENTS\n",PANGO::_fragments.size());
    for(auto & f:PANGO::_fragments) {
        f.clear();
//        PANGO_PRINT("CLEAR FRAG from %08X len=%d frag=%d FH=%u\n",(void*) f.data,f.len,f.frag,ESP.getFreeHeap());
    }
    _fragments.clear();
    _fragments.shrink_to_fit();
}

void PANGO::_clearQ(PANGO_MSG_Q* q){
    while(!(q->empty())) {
        mb tmp=q->front();
        q->pop();
        tmp.clear();
    }
}

PANGO_REM_LENGTH PANGO::_getRemainingLength(uint8_t* p){ // move to asmq
    uint32_t multiplier = 1;
    uint32_t value = 0;
    uint8_t encodedByte,len=0;
    do{
        encodedByte = *p++;
        len++;
        value += (encodedByte & 0x7f) * multiplier;
        multiplier *= 128;
    } while ((encodedByte & 0x80) != 0);
    return std::make_pair(value,len);
}

void PANGO::_release(mb m){
    if(m.len>_space) {
        uint16_t nFrags=m.len/_space+((m.len%_space) ? 1:0); // so we can mark the final fragment
        size_t bytesLeft=m.len;
        do{
            size_t toSend=std::min(_space,bytesLeft);
            ESP.wdtFeed(); // makes all the difference! First time EVER I have needed it
//            PANGO_PRINT("OUTBOUND CHUNK len=%d space=%d BL=%d\n",toSend,_space,bytesLeft);
            TXQ.push(mb(toSend,m.data+(m.len - bytesLeft),m.id,(--nFrags) ? (ADFP) nFrags:m.data,true)); // very naughty, but works :)
            bytesLeft-=toSend;
        } while(bytesLeft);
        TXQ.pop(); // hara kiri - queue is now n smaller copies of you!
        //_txPacket(TXQ.front());
        _runTXQ();
    } else _send(m);
}

void ICACHE_RAM_ATTR PANGO::_runTXQ(){ if(TCP->canSend() && !TXQ.empty()) _release(TXQ.front()); }// DON'T POP Q!!! - gets popped when sent packet is ACKed

void ICACHE_RAM_ATTR PANGO::_runRXQ(){
    if(!RXQ.empty()) {
        mb tmp=RXQ.front();
        RXQ.pop();
        LIN->_handlePacket(tmp);
    }
}

void PANGO::_saveFragment(mb m){
    uint8_t* frag=static_cast<uint8_t*>(malloc(m.len));
    memcpy(frag,m.data,m.len);
//    PANGO_PRINT("SAVE FRAGMENT (%d) from %08X -> %08X\n",m.len,m.data,frag); // copy Q
    _fragments.push_back(mb(m.len,frag,0,(ADFP) _fragments.size(),true));
}

void PANGO::_send(mb m){
    PANGO_PRINT("----> SEND %s %d bytes on wire\n",PANGO::getPktName(m.data[0]),m.len);
    TCP->add((const char*) m.data,m.len); // ESPAsyncTCP is WRONG on this, it should be a uint8_t*
    TCP->send();
    _nPollTicks=0;
}

void ICACHE_RAM_ATTR PANGO::_rxPacket(mb m){
   RXQ.push(m);
   _runRXQ();
}
void ICACHE_RAM_ATTR PANGO::_txPacket(mb m){
   TXQ.push(m);
   _runTXQ();
}
//
//  PUBLIC
//
void PANGO::dumphex(uint8_t* mem, size_t len,uint8_t W) {
    uint8_t* src = mem;
    Serial.printf("Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
    for(uint32_t i = 0; i < len; i++) {
        if(i % W == 0) Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
        Serial.printf("%02X ", *src);
        src++;
        //
        if(i % W == W-1 || src==mem+len){
            size_t ff=W-((src-mem) % W);
            for(int p=0;p<(ff % W);p++) Serial.print("   ");
            Serial.print("  "); // stretch this for nice alignment of final fragment
            for(uint8_t* j=src-(W-(ff%W));j<src;j++) Serial.printf("%c", isprint(*j) ? *j:'.');
        }
    }
    Serial.println();
}

char* PANGO::payloadToCstring(uint8_t* data,size_t len){
    char* buf=static_cast<char*>(malloc(len+1)); /// CALLER MUST FREE THIS!!!
    while(*buf++=(char)(*data++));
    buf[len]='\0';
    return buf;
};

int PANGO::payloadToInt(uint8_t* data,size_t len){
    char* c=payloadToCstring(data,len);
    int i=atoi(c);
    free(c); // as all goood programmers MUST!
    return i;
}
std::string PANGO::payloadToStdstring(uint8_t* data,size_t len){
    char* c=payloadToCstring(data,len);
    std::string s;
    s.assign(c,len);
    free(c); // as all goood programmers MUST!
    return s;
}
String PANGO::payloadToString(uint8_t* data,size_t len){
    return String(payloadToStdstring(data,len).c_str());
}

#ifdef PANGO_DEBUG
void PANGO::dump(){ 
    PANGO_PRINT("DUMP ALL %d POOL BLOX\n",mb::pool.size());
    for(auto & p:mb::pool) PANGO_PRINT("%08X\n",p);

    if(PANGO::TXQ.size()){
        PANGO_PRINT("DUMP ALL %d TX PACKETS INFLIGHT\n",PANGO::TXQ.size());
        PANGO_MSG_Q cq=PANGO::TXQ;
        while(!cq.empty()){
            cq.front().dump();
            cq.pop();
        }
    } else PANGO_PRINT("TXQ EMPTY\n");

    if(PANGO::RXQ.size()){
        PANGO_PRINT("DUMP ALL %d RX PACKETS INFLIGHT\n",PANGO::RXQ.size());
        PANGO_MSG_Q cq=PANGO::RXQ;
        while(!cq.empty()){
            cq.front().dump();
            cq.pop();
        }
    } else PANGO_PRINT("RXQ EMPTY\n");

    PANGO_PRINT("DUMP ALL %d PACKETS OUTBOUND\n",Packet::_outbound.size());
    for(auto & p:Packet::_outbound) p.second.dump();

    PANGO_PRINT("DUMP ALL %d PACKETS INBOUND\n",Packet::_inbound.size());
    for(auto & p:Packet::_inbound) p.second.dump();

    PANGO_PRINT("DUMP ALL %d FRAGMENTS\n",_fragments.size());
    for(auto & p:_fragments) p.dump();

    PANGO_PRINT("\n");
}
#else
void PANGO::dump(){}
#endif