#include<mqTraits.h>
//#include<AardvarkTCP.h>

#if PANGO_DEBUG
    std::map<uint8_t,char*> mqttTraits::pktnames={
        {0x10,"CONNECT"},
        {0x20,"CONNACK"},
        {0x30,"PUBLISH"},
        {0x40,"PUBACK"},
        {0x50,"PUBREC"},
        {0x60,"PUBREL"},
        {0x70,"PUBCOMP"},
        {0x80,"SUBSCRIBE"},
        {0x90,"SUBACK"},
        {0xa0,"UNSUBSCRIBE"},
        {0xb0,"UNSUBACK"},
        {0xc0,"PINGREQ"},
        {0xd0,"PINGRESP"},
        {0xe0,"DISCONNECT"}
    };
#endif

string mqttTraits::_decodestring(uint8_t** p){
    size_t tlen=_peek16(*p);//payload+=2;
    string rv((const char*) *(p)+2,tlen);
    *p+=2+tlen;
    return rv;
}

mqttTraits::mqttTraits(uint8_t* p,size_t s): data(p),len(s){
    type=data[0];
    flags=(data[0] & 0xf);
//  CALCULATE RL
    uint32_t multiplier = 1;
    uint8_t encodedByte;//,rl=0;
    uint8_t* pp=&data[1];
    do{
        encodedByte = *pp++;
        offset++;
        remlen += (encodedByte & 0x7f) * multiplier;
        multiplier *= 128;
    } while ((encodedByte & 0x80) != 0);

#if PANGO_DEBUG
    if(s!=1+offset+remlen) PANGO_PRINT4("SANITY FAIL! s=%d RL=%d offset=%d L=%d\n",s,remlen,offset,1+offset+remlen);
    else PANGO_PRINT4("LL s=%d RL=%d offset=%d L=%d\n",s,remlen,offset,1+offset+remlen);
#endif

    switch(type){
        case PUBACK:
        case PUBREC:
        case PUBREL:
        case PUBCOMP:
        case SUBSCRIBE:
        case SUBACK:
        case UNSUBSCRIBE:
        case UNSUBACK:
            id=_peek16(start());
            break;
        default:
            {
                if(isPublish()){
                    retain=flags & 0x01;
                    dup=(flags & 0x8) >> 3;
                    qos=(flags & 0x6) >> 1;
                    payload=start();
                    topic=_decodestring(&payload);
                    if(qos){ id=_peek16(payload);payload+=2; }
                    plen=data+len-payload;
                }
            }
            break;
    }

#if PANGO_DEBUG
    PANGO_PRINT1("MQTT %s\n",getPktName().data());
    PANGO_PRINT2(" Data @ 0x%08x len=%d RL=%d\n",data,len,remlen);
    PANGO_DUMP4(data,len);
    switch(type){
        case CONNECT:
            {
                uint8_t cf=data[9];
                PANGO_PRINT3("  Protocol: %s\n",data[8]==4 ? "3.1.1":stringFromInt(data[8],"0x%02x").data());
                PANGO_PRINT4("  Flags: %02x\n",cf);
                PANGO_PRINT3("  Session: %s\n",((cf & CLEAN_SESSION) >> 1) ? "Clean":"Dirty");
                PANGO_PRINT3("  Keepalive: %d\n",_peek16(&data[10]));
                uint8_t* sp=&data[12];
                PANGO_PRINT3("  ClientId: 0x%08x %s\n",sp,_decodestring(&sp).data());
                if(cf & WILL){
                    PANGO_PRINT3("  Will Topic: 0x%08x %s\n",sp,_decodestring(&sp).data());
                    if(cf & WILL_RETAIN) PANGO_PRINT3("  Will: RETAIN\n");
                    PANGO_PRINT3("  Will QoS: %d\n",(cf >> 3) &0x3);
                } 
                if(cf & USERNAME) PANGO_PRINT3("  Username: %s\n",_decodestring(&sp).data());
                if(cf & PASSWORD) PANGO_PRINT3("  Password: %s\n",_decodestring(&sp).data());
                break;
            }
        case CONNACK:
            {
                switch(data[3]){
                    case 0: // 0x00 Connection Accepted
                        PANGO_PRINT3("  Session: %s\n",((data[2]) & 1) ? "Present":"None");
                        break;
                    case 1: // 0x01 Connection Refused, unacceptable protocol version
                        PANGO_PRINT3("  Error: %s\n","unacceptable protocol version");
                        break;
                    case 2: // 0x02 Connection Refused, identifier rejected
                        PANGO_PRINT3("  Error: %s\n","client identifier rejected");
                        break;
                    case 3: // 0x03 Connection Refused, Server unavailable
                        PANGO_PRINT3("  Error: %s\n","Server unavailable");
                        break;
                    case 4: // 0x04 Connection Refused, bad user name or password
                        PANGO_PRINT3("  Error: %s\n","bad user name or password");
                        break;
                    case 5: // 0x05 Connection Refused, not authorized
                        PANGO_PRINT3("  Error: %s\n","not authorized");
                        break;
                    default: // ??????
                        PANGO_PRINT3("  SOMETHING NASTY IN THE WOODSHED!!\n");
                        break;
                }
            }
            break;
        case SUBSCRIBE:
        case UNSUBSCRIBE:
            {
                PANGO_PRINT3("  id: %d\n",id);
                uint8_t* payload=start()+2;
                do {
                    uint16_t len=_peek16(payload);
                    payload+=2;
                    string topic((const char*) payload,len);
                    payload+=len;
                    if(type==SUBSCRIBE) {
                        uint8_t qos=*payload++;
                        PANGO_PRINT3("  Topic: QoS%d %s\n",qos,topic.data());
                    } 
                    else PANGO_PRINT3("  Topic: %s\n",topic.data());
                } while (payload < (data + len));
            }
            break;
        default:
            {
                if(isPublish()){
                    if(qos) PANGO_PRINT3("  id: %d\n",id);
                    PANGO_PRINT3("  qos: %d\n",qos);
                    if(dup) PANGO_PRINT3("  DUP\n");
                    if(retain) PANGO_PRINT3("  RETAIN\n");
                    PANGO_PRINT3("  Topic: %s\n",topic.data());
                    PANGO_PRINT3("  Payload size: %d\n",plen);
                }
            }
            break;
    }
}

void mqttTraits::dump(){
    Serial.printf("PKTDUMP %s @ 0x%08x len=%d RL=%d off=%d flags=0x%02x\n",getPktName().data(),data,len,remlen,offset,flags);
    Serial.printf("PKTDUMP %s id=%d qos=%d dup=%d ret=%d PR=%d PL=0x%08x L=%d \n",topic.data(),id,qos,dup,retain,pubrec,payload,plen);
    dumphex(data,len);
}
#else
}
#endif