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
#include <PangolinMQTT.h>

enum ASYNC_MQTT_CNX_FLAG : uint8_t {
    USERNAME      = 0x80,
    PASSWORD      = 0x40,
    WILL_RETAIN   = 0x20,
    WILL_QOS1     = 0x08,
    WILL_QOS2     = 0x10,
    WILL          = 0x04,
    CLEAN_SESSION = 0x02
};

class Packet {
    friend class PangolinMQTT;
    protected:
        static  uint16_t         _nextId;
                uint16_t         _id=0; 
                uint8_t          _hdrAdjust;
                bool             _hasId=false;
                uint8_t          _controlcode;
                PANGO_BLOCK_Q     _blox;
                uint32_t         _bs=0;
                PANGO_FN_VOID     _begin=[]{};
                PANGO_FN_U8PTRU8  _middle=[](uint8_t* p){ return p; };
                PANGO_FN_U8PTR    _end=[](uint8_t* p,mb* base){};

        static  void             _ACK(PANGO_PACKET_MAP* m,uint16_t id,bool inout); // inout true=INBOUND false=OUTBOUND
                uint8_t*         _block(size_t size);
                void	         _build(bool hold=false);
        static  void             _resendPartialTxns();
                void             _idGarbage(uint16_t id);
                void             _initId();
                uint8_t*         _mem(const void* v,size_t size);
                uint8_t*         _poke16(uint8_t* p,uint16_t u);
                std::vector<uint8_t> _rl(uint32_t X);
                void             _shortGarbage();
                uint8_t*         _stringblock(const std::string& s){ return _mem(s.data(),s.size()); }
            
    public:
        static  PANGO_PACKET_MAP  _outbound;
        static  PANGO_PACKET_MAP  _inbound;

        Packet(uint8_t controlcode,uint8_t adj=0,bool hasid=false): _controlcode(controlcode),_hdrAdjust(adj),_hasId(hasid){}

        static  void             ACKoutbound(uint16_t id){ /*PANGO_PRINT("ACK O/B %d\n",id);*/ _ACK(&_outbound,id,false); }
};
class ConnectPacket: public Packet {
            uint8_t  protocol[8]={0x0,0x4,'M','Q','T','T',4,0}; // 3.1.1
    public:
        ConnectPacket();
};
class PingPacket: public Packet {
    public:
        PingPacket(): Packet(PINGREQ) { _shortGarbage(); }
};
class DisconnectPacket: public Packet {
    public:
        DisconnectPacket(): Packet(DISCONNECT) { _shortGarbage(); }
};
class PubackPacket: public Packet {
    public:
        PubackPacket(uint16_t id): Packet(PUBACK) { /*PANGO_PRINT("OUT PUBACK FOR ID %d\n",id);*/ _idGarbage(id); }
};
class PubrecPacket: public Packet {
    public:
        PubrecPacket(uint16_t id): Packet(PUBREC) { /*PANGO_PRINT("OUT PUBREC FOR ID %d\n",id);*/ _idGarbage(id); }
};
class PubrelPacket: public Packet {
    public:
        PubrelPacket(uint16_t id): Packet(PUBREL) { /*PANGO_PRINT("OUT PUBREL FOR ID %d\n",id);*/ _idGarbage(id); }
};
class PubcompPacket: public Packet {
    public:
        PubcompPacket(uint16_t id): Packet(PUBCOMP) { /*PANGO_PRINT("OUT PUBCOMP FOR ID %d\n",id);*/ _idGarbage(id); }  
};
class SubscribePacket: public Packet {
        std::string          _topic;
    public:
        uint8_t         _qos;
        SubscribePacket(const std::string& topic,uint8_t qos): _topic(topic),_qos(qos),Packet(SUBSCRIBE,1,true) {
            _id=++_nextId;
            _begin=[this]{ _stringblock(CSTR(_topic)); };
            _end=[this](uint8_t* p,mb* base){ *p=_qos; };
            _build();
        }
};
class UnsubscribePacket: public Packet {
        std::string          _topic;
    public:
        UnsubscribePacket(const std::string& topic): _topic(topic),Packet(UNSUBSCRIBE,0,true) {
            _id=++_nextId;
            _begin=[this]{ _stringblock(CSTR(_topic)); };
            _build();
        }
};

class PublishPacket: public Packet {
        std::string     _topic;
        uint8_t         _qos;
        bool            _retain;
        size_t          _length;
        bool            _dup;
        uint16_t        _givenId=0;
    public:
        PublishPacket(const char* topic, uint8_t qos, bool retain, uint8_t* payload, size_t length, bool dup,uint16_t givenId=0);
};
