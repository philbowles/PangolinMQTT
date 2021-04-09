#include <smartClient.h>
#include <H4Utils.h>

//**************************************************************************************************************
smartClient::smartClient():
    //_readyState(readyStateUnsent)
    //, _HTTPcode(0)
    //, _chunked(false)
    //, _timeout(DEFAULT_RX_TIMEOUT)
    //, _lastActivity(0)
    //, _requestStartTime(0)
    //, _requestEndTime(0)
     _URL(nullptr)
    , _connectedHost(nullptr)
    , _connectedPort(-1)
    //, _contentLength(0)
    //, _contentRead(0)
    //, _readyStateChangeCB(nullptr)
    //, _readyStateChangeCBarg(nullptr)
    //, _onDataCB(nullptr)
    //, _onDataCBarg(nullptr)
    //, _request(nullptr)
    //, _response(nullptr)
    //, _chunks(nullptr)
    //, _headers(nullptr)
{}
#define CSTR(x) x.c_str()

//**************************************************************************************************************
smartClient::~smartClient(){
    close(true);
    if(_URL) delete _URL;
//    delete _headers;
//    delete _request;
//    delete _response;
//    delete _chunks;
//    delete[] _connectedHost;
}
void  smartClient::_parseURL(const string& url){
    if(url.find("http",0)) _parseURL(string("http://")+url);
    else {
        bool insecure=url.find("https",0);
        vector<string> vs=split(url,"//");
        _URL = new URL;
        _URL->scheme = new char[vs[0].size()+3];
        strcpy(_URL->scheme,(vs[0]+"//").c_str());
        Serial.printf("scheme %s\n", _URL->scheme);
        vector<string> vs2=split(vs[1],"?");

        string query=vs2.size()>1 ? vs2[1]:"";
        _URL->query = new char[1+(query.size())];
        strcpy(_URL->query,CSTR(query));
        Serial.printf("query %s\n", _URL->query);

        vector<string> vs3=split(vs2[0],"/");
        string path=vs3.size()>1 ? string("/")+join(vector<string>(++vs3.begin(),vs3.end()),"/"):"";

        _URL->path = new char[1+(path.size())];
        strcpy(_URL->path,CSTR(path));
        Serial.printf("path %s\n", _URL->path);

        vector<string> vs4=split(vs3[0],":");

        _URL->port=vs4.size()>1 ? atoi(CSTR(vs4[1])):80;
        Serial.printf("port %d\n", _URL->port);

        _URL->host=new char[1+vs4[0].size()];
        strcpy(_URL->host,CSTR(vs4[0]));
        Serial.printf("host %s\n\n",_URL->host);
    }
}
/*
bool  smartClient::_connect(){
    delete[] _connectedHost;	
    _connectedHost = new char[strlen(_URL->host) + 1];
    strcpy(_connectedHost, _URL->host);
    _connectedPort = _URL->port;

    onConnect([=](void *obj, AsyncClient *client){ 
        Serial.printf("connected to %s%s:%d \n",_URL->scheme,_URL->host,_URL->port);
    });

    onDisconnect([=](void *obj, AsyncClient *client){ 
        Serial.printf("disconnected from %s%s:%d \n",_URL->scheme,_URL->host,_URL->port);
    });

    onError([=](void *obj, AsyncClient *client, uint32_t error){ 
        Serial.printf("Error %d \n",error);
    });

//    onDisconnect([](void *obj, AsyncClient* client){((smartClient*)(obj))->_onDisconnect(client);}, this);
//    onPoll([](void *obj, AsyncClient *client){((smartClient*)(obj))->_onPoll(client);}, this);
//    onError([](void *obj, AsyncClient *client, uint32_t error){((smartClient*)(obj))->_onError(client, error);}, this);

    if( !connected()){
        if( ! connect(_URL->host, _URL->port, !_URL->insecure)) return false;
    }
    else {
//        _onConnect(_client);
        Serial.printf("WAHEY!!!!!!!!!! connected to %s%s:%d \n",_URL->scheme,_URL->host,_URL->port);
    }
//    _lastActivity = millis();
    return true;
}
*/