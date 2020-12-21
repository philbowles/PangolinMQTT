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
#include<PangolinMQTT.h>
#include<unordered_set>

using PANG_MEM_POOL         = std::unordered_set<ADFP>;

class mb {
        bool                    managed=false;
        inline void             _deriveQos(){ qos=(data[0] & 0x6 ) >> 1; retries=PANGO::_maxRetries; }

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
        mb(ADFP,bool manage=true); // create skeleton mb from inbound raw data - the usual case
        mb(size_t l,uint8_t* d,uint16_t i=0,ADFP f=nullptr,bool manage=false);

                void            ack();
                void            clear();
                bool inline     isPub(){ return (data[0] & 0xf0) == 0x30; };
                void            manage();
        inline  uint8_t*        start() { return data+1+offset; }
        void dump();
};
