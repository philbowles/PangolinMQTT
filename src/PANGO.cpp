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
#include<PangolinMQTT.h>
#include<Packet.h> // remove this by moving packet statics into asmq

namespace PANGO {
            AsyncClient*        TCP;
            PANGO_MSG_Q         TXQ;
            PangolinMQTT*       LIN;
            PANGO_FRAGMENTS     _fragments={};
            bool                _inflight=false;
            uint16_t            _maxRetries=PANGO_MAX_RETRIES;
            uint32_t            _nPollTicks;
            uint32_t            _nSrvTicks;
            bool                _secure=false;
            size_t              _space=536; // rogue starting value as true value not known untill after connect!

            void                _HAL_feedWatchdog();
            uint32_t            _HAL_getFreeHeap();
            const char*         _HAL_getUniqueId();

//            size_t              _ackSize(size_t N);

            void                _ackTCP(size_t len, uint32_t time);
            void                _clearFragments();
            void                _clearQ(PANGO_MSG_Q*);
            PANGO_REM_LENGTH    _getRemainingLength(uint8_t* p);
            uint16_t            _peek16(uint8_t* p){ return (*(p+1))|(*p << 8); }
            void                _release(mb);
            void                _resetPingTimers(){ /*Serial.printf("RPT!!! \n");*/_nPollTicks=_nSrvTicks=0; }
            void                _runTXQ();
            void                _saveFragment(mb);
            void                _send(mb);
            void                _txPacket(mb);
//
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
            void            dump(); // null if no PANGO_DEBUG
            char*           getPktName(uint8_t type){
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

#if defined(ARDUINO_ARCH_STM32)
#include <STM32Ethernet.h>
void PANGO::_HAL_feedWatchdog(){}
uint32_t PANGO::_HAL_getFreeHeap(){ return 30000; }
const char* PANGO::_HAL_getUniqueId(){
    static char buf[13]="";
    uint8_t *mac;
    mac = Ethernet.MACAddress();
    sprintf(buf,"stm32-%02X%02X%02X",mac[3],mac[4],mac[5]);
    return buf;
}
#elif defined(ARDUINO_ARCH_ESP32)
void PANGO::_HAL_feedWatchdog(){}
uint32_t PANGO::_HAL_getFreeHeap(){ return ESP.getFreeHeap(); }
const char* PANGO::_HAL_getUniqueId(){
    static char buf[19];
    sprintf(buf, "esp32-%12llX", ESP.getEfuseMac());
    return buf;
}
#else
void PANGO::_HAL_feedWatchdog(){ ESP.wdtFeed(); }
uint32_t PANGO::_HAL_getFreeHeap(){ return ESP.getFreeHeap(); }
const char* PANGO::_HAL_getUniqueId(){
    static char buf[15];
    sprintf(buf, "esp8266-%06X", ESP.getChipId());
    return buf;
}
#endif

void PANGO::_ackTCP(size_t len, uint32_t time){
    PANGO_PRINT("TXQ=%d TCP ACK LENGTH=%d\n",TXQ.size(),len);
    _resetPingTimers();
    size_t amtToAck=len;
    while(amtToAck){
        if(!TXQ.empty()){
            mb tmp=TXQ.front();
            TXQ.pop();
            PANGO_PRINT("TXQ=%d TCP frag ACK LENGTH=%d acksize=%d amtleft=%d\n",TXQ.size(),tmp.len,69+((tmp.len>>4)<<4),amtToAck);
            amtToAck-=_secure ? 69+((tmp.len>>4)<<4):tmp.len;//_ackSize(tmp.len);
            tmp.ack();
        } else break;
    }
    _runTXQ();
}

void PANGO::_clearFragments(){
//    PANGO_PRINT("CLEAR %d FRAGMENTS\n",PANGO::_fragments.size());
    for(auto & f:PANGO::_fragments) f.clear();
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
            _HAL_feedWatchdog();
//            PANGO_PRINT("OUTBOUND CHUNK len=%d space=%d BL=%d\n",toSend,_space,bytesLeft);
            TXQ.push(mb(toSend,m.data+(m.len - bytesLeft),m.id,(--nFrags) ? (ADFP) nFrags:m.data,true)); // very naughty, but works :)
            bytesLeft-=toSend;
        } while(bytesLeft);
        TXQ.pop(); // hara kiri - queue is now n smaller copies of yourself!
        _runTXQ();
    } else _send(m);
}

void  PANGO::_runTXQ(){ if(!TXQ.empty()) _release(TXQ.front()); } // DON'T POP Q!!! - gets popped when sent packet is ACKed

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
}

void  PANGO::_txPacket(mb m){
    TXQ.push(m);
    if(TXQ.size()==1) _release(m);
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
    uint8_t* buf=static_cast<uint8_t*>(malloc(len)); /// CALLER MUST FREE THIS!!!
    memcpy(buf,data,len);
    return (char*) buf;
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

    PANGO_PRINT("DUMP ALL %d PACKETS OUTBOUND\n",Packet::_outbound.size());
    for(auto & p:Packet::_outbound) p.second.dump();

    PANGO_PRINT("DUMP ALL %d PACKETS INBOUND\n",Packet::_inbound.size());
    for(auto & p:Packet::_inbound) p.second.dump();

    PANGO_PRINT("DUMP ALL %d FRAGMENTS\n",_fragments.size());
    for(auto & p:_fragments) p.dump();

    PANGO_PRINT("\n");
}
#endif