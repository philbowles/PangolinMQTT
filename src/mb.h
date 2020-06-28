#pragma once
#include<PangolinMQTT.h>
#include<unordered_set>

using PANG_MEM_POOL         = std::unordered_set<ADFP>;

class mb {
        bool                    managed=false;
        inline void             _deriveQos(){ qos=(data[0] & 0x6 ) >> 1; retries=PANGO::_maxRetries; }
//        inline void             _deriveQos(){ qos=(data[0] & 0x6 ) >> 1; }

    public:
        static  PANG_MEM_POOL   pool;
                size_t          len=0;
                ADFP            data=nullptr;
                uint8_t         qos=0;
                uint8_t         offset=0;
                uint16_t        id=0;
                ADFP            frag=nullptr;
                uint8_t         retries;
                bool            dup;
                bool            retain;
                std::string     topic;
                uint8_t*        payload;
                uint32_t        plen;
                bool            pubrec=false;

//        ~mb(){} // ABSOLUTELY DO NOT EVER NOT NEVER NOHOW FREE data HERE!!!!

        mb(){ retries=PANGO::_maxRetries;managed=true; /* PANGO_PRINT("DEFAULT CTOR managed=%d\n",managed); */ }; // only in raw packet construction
//        mb(){ managed=true; /* PANGO_PRINT("DEFAULT CTOR managed=%d\n",managed); */ }; // only in raw packet construction
        mb(ADFP,bool manage=true); // create skeleton mb from inbound raw data - the usual case
        mb(size_t l,uint8_t* d,uint16_t i=0,ADFP f=nullptr,bool manage=false);

                void            ack();
                void            clear();
                bool inline     isPub(){ return (data[0] & 0xf0) == 0x30; };
                void            manage();
        inline  uint8_t*        start () { return data+1+offset; }
//#ifdef PANGO_DEBUG
        void dump();
//#endif
};
