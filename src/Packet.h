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
#include <mqTraits.h>

using PANGO_BLOCK_Q        = std::queue<mbx>;

class Packet {
    friend class PangolinMQTT;
    protected:
        static  uint16_t         _nextId;
                uint16_t         _id=0; 
                bool             _hasId=false;
                uint8_t          _controlcode;
                PANGO_BLOCK_Q    _blox;
                uint32_t         _bs=0;
                PANGO_FN_VOID    _begin=[]{};
                PANGO_FN_U8PTRU8 _middle=[](uint8_t* p){ return p; };
                PANGO_FN_U8PTR   _end=[](uint8_t* p,uint8_t* base){}; // lose this?

                void	         _build(bool hold=false);
                void             _idGarbage(uint16_t id);
                void             _initId();
                void             _multiTopic(std::initializer_list<const char*> topix,uint8_t qos=0);
                uint8_t*         _poke16(uint8_t* p,uint16_t u);
                void             _stringblock(const std::string& s);
    public:
        Packet(uint8_t controlcode,bool hasid=false): _controlcode(controlcode),_hasId(hasid){}
};
class ConnectPacket: public Packet {
            uint8_t  protocol[8]={0x0,0x4,'M','Q','T','T',4,0}; // 3.1.1
    public:
        ConnectPacket();
};

struct PubackPacket: public Packet {
    public:
        PubackPacket(uint16_t id): Packet(PUBACK) { _idGarbage(id); }
};
class PubrecPacket: public Packet {
    public:
        PubrecPacket(uint16_t id): Packet(PUBREC) { _idGarbage(id); }
};
class PubrelPacket: public Packet {
    public:
        PubrelPacket(uint16_t id): Packet(PUBREL) { _idGarbage(id); }
};
class PubcompPacket: public Packet {
    public:
        PubcompPacket(uint16_t id): Packet(PUBCOMP) { _idGarbage(id); }  
};
class SubscribePacket: public Packet {
    public:
        SubscribePacket(const std::string& topic,uint8_t qos): Packet(SUBSCRIBE,true) { _multiTopic({topic.data()},qos); }
        SubscribePacket(std::initializer_list<const char*> topix,uint8_t qos): Packet(SUBSCRIBE,true) { _multiTopic(topix,qos); }
};

class UnsubscribePacket: public Packet {
    public:
        UnsubscribePacket(const std::string& topic): Packet(UNSUBSCRIBE,true) { _multiTopic({topic.data()}); }
        UnsubscribePacket(std::initializer_list<const char*> topix): Packet(UNSUBSCRIBE,true) { _multiTopic(topix); }
};

class PublishPacket: public Packet {
        std::string     _topic;
        uint8_t         _qos;
        bool            _retain;
        size_t          _length;
        bool            _dup;
        uint16_t        _givenId=0;
    public:
        PublishPacket(const char* topic, uint8_t qos, bool retain, const uint8_t* payload, size_t length, bool dup,uint16_t givenId=0);
};
