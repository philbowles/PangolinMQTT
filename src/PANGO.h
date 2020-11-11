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

class mb;
class PangolinMQTT;
class AsynClient;

using PANGO_FRAGMENTS      = std::vector<mb>;
using PANGO_MSG_Q          = std::queue<mb>;

namespace PANGO {
    extern  AsyncClient*     TCP;
    extern  PangolinMQTT*    LIN;
    extern  PANGO_MSG_Q      TXQ;
    extern  PANGO_FRAGMENTS  _fragments;
    extern  uint16_t         _maxRetries;
    extern  uint32_t         _nPollTicks;
    extern  uint32_t         _nSrvTicks;
    extern  bool             _secure;
    extern  size_t           _space;
//
//  NEVER call anything that stats with underscore! "_"
//
    extern void             _HAL_feedWatchdog();
    extern uint32_t         _HAL_getFreeHeap();
    extern const char*      _HAL_getUniqueId();

    extern void             _ackTCP(size_t len, uint32_t time);
    extern void             _clearFragments();
    extern void             _clearQ(PANGO_MSG_Q*);
    extern uint16_t         _peek16(uint8_t* p);
    extern void             _resetPingTimers();
    extern void             _runTXQ();
    extern void             _saveFragment(mb);
    extern void             _send(mb);
    extern void             _txPacket(mb);

    extern void             dumphex(const uint8_t* mem, size_t len,uint8_t W=16);

#if PANGO_DEBUG
    extern void             dump(); // null if no debug
    extern char*            getPktName(uint8_t type);
#endif
}

#ifdef PANGO_DEBUG
    template<int I>
    void pango_dump(const uint8_t* p, size_t len) { if (PANGO_DEBUG >= I) PANGO::dumphex(p,len,16); }
    #define PANGO_DUMP3(p,l) pango_dump<3>((p),l)
    #define PANGO_DUMP4(p,l) pango_dump<4>((p),l)
#else
  #define PANGO_DUMP3(...)
  #define PANGO_DUMP4(...)
#endif
