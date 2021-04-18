#pragma once
#include<AardvarkUtils.h>

#include<map>
#include<string>

enum :uint8_t {
    CONNECT     = 0x10, // x
    CONNACK     = 0x20, // x
    PUBLISH     = 0x30, // x
    PUBACK      = 0x40, // x
    PUBREC      = 0x50, 
    PUBREL      = 0x62,
    PUBCOMP     = 0x70,
    SUBSCRIBE   = 0x82, // x
    SUBACK      = 0x90, // x
    UNSUBSCRIBE = 0xa2, // x
    UNSUBACK    = 0xb0, // x
    PINGREQ     = 0xc0, // x
    PINGRESP    = 0xd0, // x
    DISCONNECT  = 0xe0
};

class mqttTraits {
                uint32_t        remlen=0;
                uint8_t         offset=0;
                uint8_t         flags=0;
                
                string          _decodestring(uint8_t** p);
        inline  uint16_t        _peek16(uint8_t* p){ return (*(p+1))|(*p << 8); }
/* a step too far
        inline  uint16_t        X_peek16(uint8_t** p){
            uint8_t lo=*(*p)++;
            uint8_t hi=*(*p)++;
            return (uint16_t) ((lo << 8)|hi);
        }
*/
    public:
                uint8_t*        data;
                size_t          len;
                uint8_t         type;
                uint16_t        id=0;
                uint8_t         qos=0;
                bool            dup;
                bool            retain;
                string          topic;
                uint8_t*        payload;
                size_t          plen;
                bool            pubrec;
                size_t          retries=PANGO_MAX_RETRIES;
                
#if PANGO_DEBUG
        static std::map<uint8_t,char*> pktnames;
        std::string             getPktName(){
            uint8_t t=type&0xf0;
            if(pktnames.count(t)) return pktnames[t];
            else return stringFromInt(type,"02X");
        }
                void            dump();
#else
        std::string             getPktName(){ return stringFromInt(type,"02X"); }
//                void            dump(){};
#endif
        mqttTraits(){};
        mqttTraits(uint8_t* p,size_t s);

        inline  bool            isPublish() { return (type & 0xf0) == PUBLISH; }
        inline  uint8_t*        start() { return data+1+offset; }
};
