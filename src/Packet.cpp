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
#include<Packet.h>

uint16_t                            Packet::_nextId=1000;

void Packet::_build(bool hold){
    uint8_t* virgin;
    _begin();
    if(_hasId) _bs+=2;
    // calc rl
    uint32_t X=_bs;
    std::vector<uint8_t> rl;
    uint8_t encodedByte;
    do{
        encodedByte = X % 128;
        X = X / 128;
        if ( X > 0 ) encodedByte = encodedByte | 128;
        rl.push_back(encodedByte);
    } while ( X > 0 );
    _bs+=1+rl.size();
    uint8_t* snd_buf=virgin=mbx::getMemory(_bs);
    PANGO_PRINT4("PACKET CREATED @ 0x%08x len=%d\n",snd_buf,_bs);
    if(snd_buf){
        *snd_buf++=_controlcode;
        for(auto const& r:rl) *snd_buf++=r;
        if(_hasId) snd_buf=_poke16(snd_buf,_id);
        snd_buf=_middle(snd_buf);
        while(!_blox.empty()){
            mbx tmp=_blox.front();
            uint16_t n=tmp.len;
            uint8_t* p=tmp.data;
            snd_buf=_poke16(snd_buf,n);
            memcpy(snd_buf,p,n);
            snd_buf+=n;
            tmp.clear();
            _blox.pop();
        }
        _end(snd_buf,virgin);
        if(!hold) PANGOV3->txdata(virgin,_bs,false);
    }  
    else PANGOV3->_notify(NOT_ENOUGH_MEMORY,_bs);
}

void Packet::_idGarbage(uint16_t id){
    uint8_t  G[]={_controlcode,2,(id & 0xff00) >> 8,id & 0xff};
    PANGOV3->txdata(&G[0],4,true);
}

void Packet::_multiTopic(std::initializer_list<const char*> topix,uint8_t qos){
    static std::vector<std::string> topics;
    _id=++_nextId;
    _begin=[=]{
        for(auto &t:topix){
            topics.push_back(t);
            _bs+=(_controlcode==0x82 ? 3:2)+strlen(t);
        }
    };
    _middle=[=](uint8_t* p){
        for(auto const& t:topics){
            size_t n=t.size();
            p=_poke16(p,n);
            memcpy(p,t.data(),n);
            p+=n;
            if(_controlcode==0x82) *p++=qos;
        }
        return p;
    };
    _build();
    topics.clear();
    topics.shrink_to_fit();
}

uint8_t* Packet::_poke16(uint8_t* p,uint16_t u){
    *p++=(u & 0xff00) >> 8;
    *p++=u & 0xff;
    return p;
}

void Packet::_stringblock(const std::string& s){ 
    size_t sz=s.size();
    _bs+=sz+2;
    _blox.push(mbx((uint8_t*) s.data(),sz,true));
}

ConnectPacket::ConnectPacket(): Packet(CONNECT){
    _bs=10;
    _begin=[=]{
        if(PANGOV3->_cleanSession) protocol[7]|=CLEAN_SESSION;
        if(PANGOV3->_willRetain) protocol[7]|=WILL_RETAIN;
        if(PANGOV3->_willQos) protocol[7]|=(PANGOV3->_willQos==1) ? WILL_QOS1:WILL_QOS2;
        _stringblock(PANGOV3->_clientId);
        if(PANGOV3->_willTopic.size()){
            _stringblock(PANGOV3->_willTopic);
            _stringblock(PANGOV3->_willPayload);
            protocol[7]|=WILL;
        }
        if(PANGOV3->_username.size()){
            _stringblock(PANGOV3->_username);
            protocol[7]|=USERNAME;
        }
        if(PANGOV3->_password.size()){
            _stringblock(PANGOV3->_password);
            protocol[7]|=PASSWORD;
        }
    };
    _middle=[=](uint8_t* p){
        memcpy(p,&protocol,8);p+=8;
        return _poke16(p,PANGOV3->_keepalive);
    };
    _build();
}

PublishPacket::PublishPacket(const char* topic, uint8_t qos, bool retain, const uint8_t* payload, size_t length, bool dup,uint16_t givenId):
    _topic(topic),_qos(qos),_retain(retain),_length(length),_dup(dup),_givenId(givenId),Packet(PUBLISH) {
        if(length < PANGOV3->getMaxPayloadSize()){
            _begin=[this]{ 
                _stringblock(_topic.c_str());
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

            _end=[=](uint8_t* p,uint8_t* base){ 
                uint8_t* p2=_qos ? _poke16(p,_id):p;
                memcpy(p2,payload,_length);
                mqttTraits T(base,_bs);
                if(_givenId) PangolinMQTT::_inbound[_id]=T;
                else if(_qos) PangolinMQTT::_outbound[_id]=T;
            };
            _build(_givenId);
        } else PANGOV3->_notify(_givenId ? INBOUND_PUB_TOO_BIG:OUTBOUND_PUB_TOO_BIG,length);
}