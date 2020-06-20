#pragma once

#include<PangolinMQTT.h>

class mb;
class PangolinMQTT;
class AsynClient;

using PANGO_FRAGMENTS      = std::vector<mb>;
using PANGO_REM_LENGTH     = std::pair<uint32_t,uint8_t>;
using PANGO_MSG_Q          = std::queue<mb>;

namespace PANGO {
    extern  AsyncClient*     TCP;
    extern  PangolinMQTT*    LIN;
    extern  PANGO_MSG_Q      TXQ;
    extern  PANGO_MSG_Q      RXQ;
    extern  PANGO_FRAGMENTS  _fragments;
    extern  uint16_t         _maxRetries;
    extern  uint32_t         _nPollTicks;
    extern  uint32_t         _nSrvTicks;
    extern  size_t           _space;
//
//  NEVER call anything that stats with underscore! "_"
//
    extern void             _ackTCP(size_t len, uint32_t time);
    extern void             _clearFragments();
    extern void             _clearQ(PANGO_MSG_Q*);
    extern PANGO_REM_LENGTH _getRemainingLength(uint8_t* p);
    extern uint16_t         _peek16(uint8_t* p);
    extern void             _resetPingTimers();
    extern void             _runRXQ();
    extern void             _runTXQ();
    extern void             _rxPacket(mb);
    extern void             _saveFragment(mb);
    extern void             _send(mb);
    extern void             _txPacket(mb);
//
//  These are all safe to call, in fact I recommend you DO!
//
    extern void             dump(); // null if no debug
    extern void             dumphex(uint8_t* mem, size_t len,uint8_t W=16);
    extern char*            payloadToCstring(uint8_t* data,size_t len);
    extern int              payloadToInt(uint8_t* data,size_t len);
    extern std::string      payloadToStdstring(uint8_t* data,size_t len);
    extern String           payloadToString(uint8_t* data,size_t len);
#ifdef PANGO_DEBUG
    extern char*            getPktName(uint8_t type);

#endif
}
