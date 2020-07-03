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
#include"Packet.h"

uint16_t                            Packet::_nextId=1000;
PANGO_PACKET_MAP                    Packet::_inbound;
PANGO_PACKET_MAP                    Packet::_outbound;

// protect this

void Packet::_ACK(PANGO_PACKET_MAP* m,uint16_t id,bool inout){ /// refakta?
    if(m->count(id)){
        ((*m)[id]).clear(); // THIS is where the memory leaks get mopped up!
        m->erase(id);
    } else PANGO::LIN->_notify(inout ? INBOUND_QOS_ACK_FAIL:OUTBOUND_QOS_ACK_FAIL,id); //PANGO_PRINT("WHO TF IS %d???\n",id);
}

uint8_t* Packet::_block(size_t size){
    _bs+=size+2;
    _blox.push(std::make_pair(size,static_cast<uint8_t*>(malloc(size))));
    return _blox.back().second;
}

void Packet::_build(bool hold){
    mb m{}; // empty message block
    _begin();
    uint32_t sx=_bs+_hdrAdjust;
    if(_hasId) sx+=2;
    std::vector<uint8_t> rl=_rl(sx);
    sx+=1+rl.size();
 
    ADFP snd_buf=m.data=(static_cast<ADFP>(malloc(sx))); // Not a memleak - will be free'd when TCP ACKs it.
    
    *snd_buf++=_controlcode;
    for(auto const& r:rl) *snd_buf++=r;
    if(_hasId) snd_buf=_poke16(snd_buf,_id);
    snd_buf=_middle(snd_buf);
    while(!_blox.empty()){
        uint16_t n=_blox.front().first;
        uint8_t* p=_blox.front().second;
        snd_buf=_poke16(snd_buf,n);
        memcpy(snd_buf,p,n);
        snd_buf+=n;
        free(p);
        _blox.pop();
    }
    _end(snd_buf,&m);
    // the pointer in m.data MUST NOT BE FREED YET!
    // The class instance will go out of scope, its temp blocks are freed, but its built data MUST live on until ACK
    // populate fields from newly constructed bare mb
    if(!hold) PANGO::_txPacket(mb (m.data,true));
}

void Packet::_resendPartialTxns(){
    std::vector<uint16_t> morituri;
    for(auto const& o:_outbound){
        auto m=o.second;
        if(--(m.retries)){
            if(m.pubrec){
                PANGO_PRINT("WE ARE PUBREC'D ATTEMPT @ QOS2: SEND %d PUBREL\n",m.id);
                PubrelPacket prp(m.id);
            }
            else {
                m.data[0]|=0x08; // set dup & resend
                PANGO::_txPacket(m);
            }
        }
        else {
            PANGO_PRINT("NO JOY AFTER %d ATTEMPTS: QOS FAIL\n",PANGO::_maxRetries);
            morituri.push_back(m.id); // all hope exhausted TODO: reconnect?
        }
    }
   for(auto const& i:morituri) ACKoutbound(i);
}

void Packet::_idGarbage(uint16_t id){
    ADFP p=static_cast<uint8_t*>(malloc(4));
    p[0]=_controlcode;
    p[1]=2;
    _poke16(&p[2],id);
    PANGO::_txPacket(mb(p,true));
    // Do NOT free p here!!! Gets freed by mb.clear() @ ack time
}

uint8_t* Packet::_mem(const void* v,size_t size){
    if(size){
        _bs+=size+2;
        uint8_t* p=static_cast<uint8_t*>(malloc(size));
        _blox.push(std::make_pair(size,p));
        memcpy(p,v,size);
        return p;
    }
    return nullptr;
}

uint8_t* Packet::_poke16(uint8_t* p,uint16_t u){
    *p++=(u & 0xff00) >> 8;
    *p++=u & 0xff;
    return p;
}

std::vector<uint8_t> Packet::_rl(uint32_t X){
    std::vector<uint8_t> rl;
    uint8_t encodedByte;
    do{
        encodedByte = X % 128;
        X = X / 128;
        if ( X > 0 ) encodedByte = encodedByte | 128;
        rl.push_back(encodedByte);
    } while ( X> 0 );
    return rl;
}

void Packet::_shortGarbage(){
    uint8_t* p=static_cast<uint8_t*>(malloc(2));
    p[0]=_controlcode;
    p[1]=_controlcode=0;
    PANGO::_txPacket(mb(p,true));
}

ConnectPacket::ConnectPacket(): Packet(CONNECT,10){
    _begin=[this]{
        if(PangolinMQTT::_cleanSession) protocol[7]|=CLEAN_SESSION;
        if(PangolinMQTT::_willRetain) protocol[7]|=WILL_RETAIN;
        if(PangolinMQTT::_willQos) protocol[7]|=(PangolinMQTT::_willQos==1) ? WILL_QOS1:WILL_QOS2;
        uint8_t* pClientId=_stringblock(PANGO::LIN->_clientId);
        if(PangolinMQTT::_willTopic.size()){
            _stringblock(PangolinMQTT::_willTopic);
            _stringblock(PangolinMQTT::_willPayload);
            protocol[7]|=WILL;
        }
        if(PangolinMQTT::_username.size()){
            _stringblock(PangolinMQTT::_username);
            protocol[7]|=USERNAME;
        }
        if(PangolinMQTT::_password.size()){
            _stringblock(PangolinMQTT::_password);
            protocol[7]|=PASSWORD;
        }
    };
    _middle=[this](uint8_t* p){
        memcpy(p,&protocol,8);p+=8;
        return _poke16(p,PangolinMQTT::_keepalive);
    };
    _build();
}

PublishPacket::PublishPacket(const char* topic, uint8_t qos, bool retain, uint8_t* payload, size_t length, bool dup,uint16_t givenId):
    _topic(topic),_qos(qos),_retain(retain),_length(length),_dup(dup),_givenId(givenId),Packet(PUBLISH) {
        if(length < PANGO::LIN->getMaxPayloadSize()){
            _begin=[this]{ 
                _stringblock(CSTR(_topic));
                _bs+=_length;
                byte flags=_retain;
                flags|=(_dup << 3);
            //
                if(_qos) {
                    _id=_givenId ? _givenId:(++_nextId);
                    flags|=(_qos << 1);
                    _bs+=2; // because Packet id will be added
                }
                _controlcode|=flags;
            };
            _end=[this,payload](uint8_t* p,mb* base){ 
                uint8_t* p2=_qos ? _poke16(p,_id):p;
                memcpy(p2,payload,_length);
                base->qos=_qos;
                mb pub(base->data,true); // make it a proper pub-populated msgblok
                if(_givenId) _inbound[_id]=pub;
                else if(_qos) _outbound[_id]=pub;
            };
            _build(_givenId);
        } else PANGO::LIN->_notify(OUTBOUND_PUB_TOO_BIG,length);
}
