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
#include"mb.h"

PANG_MEM_POOL       mb::pool;

mb::mb(size_t l,uint8_t* d,uint16_t i,ADFP f,bool track): len(l),id(i),data(d),frag(f),managed(track){ // always unmanaged - sould only be called by onData
//    PANGO_PRINT("LONG-WINDED CTOR %08X len=%d managed=%d\n",d,l,managed);
    manage();
}

mb::mb(ADFP p, bool track): data(p),managed(track) {
//    PANGO_PRINT("SKELETON %08X TYPE %s managed=%d\n",p,PANGO::getPktName(p[0]),managed);
    std::pair<uint32_t,uint8_t> remlen=PANGO::_getRemainingLength(&data[1]); // demote
    offset=remlen.second;
//    rl=remlen.first; // mqtt "remaining length"
    len=1+offset+remlen.first; // MQTT msg size
    manage();
//    PANGO_PRINT("SKELETON %08X TYPE %s len=%d managed=%d\n",data,PANGO::getPktName(data[0]),len,managed);
//  type 0x30 only
    if(isPub()){
        uint8_t     bits=data[0] & 0x0f;
        dup=(bits & 0x8) >> 3;
        qos=(bits & 0x6) >> 1;
        retain=bits & 0x1;

        uint8_t* p=start();
        id=0;
        size_t tlen=PANGO::_peek16(p);p+=2;
        char c_topic[tlen+1];
        memcpy(&c_topic[0],p,tlen);c_topic[tlen]='\0';
        topic.assign(&c_topic[0],tlen);
        p+=tlen;
        if(qos) {
            id=PANGO::_peek16(p);
            p+=2;
        }
        plen=data+len-p;
        payload=p;
    }
}

void mb::ack(){
    if(frag){
//        PANGO_PRINT("**** FRAGGY MB %08X frag=%08X\n",data,frag);
        if((int) frag < 100) return; // some arbitrarily ridiculous max numberof _fragments
        data=frag; // reset data pointer to FIRST fragment, so whole block is freed
        _deriveQos(); // recover original QOS from base fragment
    } 
//    PANGO_PRINT("**** PROTOCOL ACK MB %08X TYPE %02X L=%d I=%d Q=%d F=%08X R=%d\n",data,data[0],len,id,qos,frag,retries);
    if(!(isPub() && qos)) clear();
//    else PANGO_PRINT("HELD MB %08X TYPE %02X L=%d I=%d Q=%d F=%08X R=%d\n",data,data[0],len,id,qos,frag,retries);
}

void mb::clear(){
    if(managed){
        if(pool.count(data)) {
 //           PANGO_PRINT("*********************************** FH=%u KILL POOL %08X %d remaining\n\n",PANGO::_HAL_getFreeHeap()),data,pool.size());
            if(data){
                free(data);
                pool.erase(data);
            } //else PANGO_PRINT("WARNING!!! ZERO MEM BLOCK %08X\n",z);
        }
//        else PANGO_PRINT("WARNING!!! DOUBLE DIP DELETE! %08X len=%d type=%02X id=%d\n",data,len,data[0],id);
    } //else PANGO_PRINT("UNMANAGED\n");
}

void mb::manage(){
//    PANGO_PRINT("MANAGE %08X\n",data);
    if(managed){
        if(!pool.count(data)) {
//            PANGO_PRINT("\n*********************************** FH=%u INTO POOL %08X TYPE %s len=%d IN Q %u\n",PANGO::_HAL_getFreeHeap()),data,PANGO::getPktName(data[0]),len,pool.size());
            pool.insert(data);
//            dump();
        } //else PANGO_PRINT("\n* NOT ***************************** INTO POOL %08X %d IN Q\n",p,pool.size());
    } //else PANGO_PRINT("* NOT MANAGED\n");
    _deriveQos();
}

#ifdef PANGO_DEBUG
void mb::dump(){
    if(data){
        PANGO_PRINT("MB %08X TYPE %02X L=%d M=%d O=%d I=%d Q=%d F=%08X\n",data,data[0],len,managed,offset,id,qos,frag);
        PANGO::dumphex(data,len);
    } else PANGO_PRINT("MB %08X ZL or bare: can't dump\n",data);
}
#else
void mb::dump(){}
#endif
