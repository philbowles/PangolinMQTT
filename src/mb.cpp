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

mb::mb(size_t l,uint8_t* d,uint16_t i,ADFP f,bool track): len(l),id(i),data(d),frag(f),managed(track){ manage(); } // always unmanaged - should only be called by onData

mb::mb(ADFP p, bool track): data(p),managed(track) {
    uint32_t multiplier = 1;
    uint32_t value = 0;
    uint8_t encodedByte;//,rl=0;
    ADFP pp=&data[1];
    do{
        encodedByte = *pp++;
        offset++;
        value += (encodedByte & 0x7f) * multiplier;
        multiplier *= 128;
    } while ((encodedByte & 0x80) != 0);
    len=1+offset+value; // MQTT msg size
    manage();
//  type 0x30 only
    if(isPub()){
        uint8_t bits=data[0] & 0x0f;
        dup=(bits & 0x8) >> 3;
        qos=(bits & 0x6) >> 1;
        retain=bits & 0x1;

        uint8_t* ps=start();
        id=0;
        size_t tlen=PANGO::_peek16(ps);ps+=2;
        char c_topic[tlen+1];
        memcpy(&c_topic[0],ps,tlen);c_topic[tlen]='\0';
        topic.assign(&c_topic[0],tlen);
        ps+=tlen;
        if(qos) {
            id=PANGO::_peek16(ps);
            ps+=2;
        }
        plen=data+len-ps;
        payload=ps;
    }
}

void mb::ack(){
    if(frag){
        if((int) frag < 100) return; // some arbitrarily ridiculous max numberof _fragments
        data=frag; // reset data pointer to FIRST fragment, so whole block is freed
        _deriveQos(); // recover original QOS from base fragment
    } 
    if(!(isPub() && qos)) clear();
}

void mb::clear(){
    if(managed){
        if(pool.count(data)) {
            if(data){
                free(data);
                pool.erase(data);
            }
        }
    }
}

void mb::manage(){
    if(managed){
        if(!pool.count(data)) pool.insert(data);
    }
    _deriveQos();
}

#ifdef PANGO_DEBUG
void mb::dump(){
    if(data){
        PANGO_PRINT4("MB %08X TYPE %02X L=%d M=%d O=%d I=%d Q=%d F=%08X\n",data,data[0],len,managed,offset,id,qos,frag);
        PANGO::dumphex(data,len);
    }
}
#else
void mb::dump(){}
#endif
