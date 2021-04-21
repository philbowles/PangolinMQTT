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
#include<Arduino.h>

extern void dumphex(const uint8_t*,size_t);

#if PANGO_DEBUG
    template<int I, typename... Args>
    void PANGO_PRINT(const char* fmt, Args... args) {
        if (PANGO_DEBUG >= I) Serial.printf(std::string(std::string("PANG:%d: ")+fmt).c_str(),I,args...);
    }
    #define PANGO_PRINT1(...) PANGO_PRINT<1>(__VA_ARGS__)
    #define PANGO_PRINT2(...) PANGO_PRINT<2>(__VA_ARGS__)
    #define PANGO_PRINT3(...) PANGO_PRINT<3>(__VA_ARGS__)
    #define PANGO_PRINT4(...) PANGO_PRINT<4>(__VA_ARGS__)

    template<int I>
    void pango_dump(const uint8_t* p, size_t len) { if (PANGO_DEBUG >= I) dumphex(p,len); }
    #define PANGO_DUMP3(p,l) pango_dump<3>((p),l)
    #define PANGO_DUMP4(p,l) pango_dump<4>((p),l)
#else
    #define PANGO_PRINT1(...)
    #define PANGO_PRINT2(...)
    #define PANGO_PRINT3(...)
    #define PANGO_PRINT4(...)

    #define PANGO_DUMP3(...)
    #define PANGO_DUMP4(...)
#endif

enum PANGO_MQTT_CNX_FLAG : uint8_t {
    USERNAME      = 0x80,
    PASSWORD      = 0x40,
    WILL_RETAIN   = 0x20,
    WILL_QOS1     = 0x08,
    WILL_QOS2     = 0x10,
    WILL          = 0x04,
    CLEAN_SESSION = 0x02
};