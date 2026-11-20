#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib,"ws2_32.lib")
#endif
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <cerrno>
#endif
#include "net_session.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <utility>

namespace {
#ifdef _WIN32
using Socket = SOCKET;
using Length = int;
constexpr Socket invalid = INVALID_SOCKET;
struct Runtime { WSADATA data{}; bool ok = WSAStartup(MAKEWORD(2,2), &data)==0; ~Runtime(){if(ok) WSACleanup();} };
bool available(){static Runtime rt; return rt.ok;}
void closeSocket(Socket s){closesocket(s);}
bool pending(){int e=WSAGetLastError(); return e==WSAEWOULDBLOCK || e==WSAEINPROGRESS || e==WSAEINTR;}
bool nonblock(Socket s){u_long v=1; return ioctlsocket(s,FIONBIO,&v)==0;}
int writable(Socket s){fd_set w,e; FD_ZERO(&w); FD_ZERO(&e); FD_SET(s,&w); FD_SET(s,&e); timeval t{}; return select(0,nullptr,&w,&e,&t);}
#else
using Socket = int;
using Length = socklen_t;
constexpr Socket invalid = -1;
bool available(){return true;}
void closeSocket(Socket s){::close(s);}
bool pending(){return errno==EAGAIN || errno==EWOULDBLOCK || errno==EINPROGRESS || errno==EINTR;}
bool nonblock(Socket s){int flags=fcntl(s,F_GETFL); return flags>=0 && fcntl(s,F_SETFL,flags|O_NONBLOCK)==0;}
int writable(Socket s){pollfd f{s,POLLOUT,0}; return ::poll(&f,1,0);}
#endif
void release(Socket& s){if(s!=invalid) closeSocket(std::exchange(s,invalid));}
using Clock=std::chrono::steady_clock;
std::string clean(std::string s,size_t limit){if(s.size()>limit)s.resize(limit); for(char& c:s) if(static_cast<unsigned char>(c)<32 || c==127)c=' '; return s;}
bool scanInt(const char*& p, int& out) {
    while (*p == ' ' || *p == '\t') ++p;
    if (!*p) return false;
    char* end = nullptr;
    const long v = std::strtol(p, &end, 10);
    if (end == p || (*end != '\0' && *end != ' ' && *end != '\t')) return false;
    p = end;
    out = static_cast<int>(v);
    return true;
}
bool scanDouble(const char*& p, double& out) {
    while (*p == ' ' || *p == '\t') ++p;
    if (!*p) return false;
    char* end = nullptr;
    const double v = std::strtod(p, &end);
    if (end == p || (*end != '\0' && *end != ' ' && *end != '\t')) return false;
    p = end;
    out = v;
    return true;
}
bool pose(const char*& p,PlayerPose& out){return scanDouble(p,out.x)&&scanDouble(p,out.y)&&scanDouble(p,out.feet)&&scanDouble(p,out.yaw)&&scanDouble(p,out.pitch)&&std::isfinite(out.x)&&std::isfinite(out.y)&&std::isfinite(out.feet)&&std::isfinite(out.yaw)&&std::isfinite(out.pitch);}
}

struct NetSession::Impl {
    Socket listener=invalid, peer=invalid;
    std::uint16_t port{}; std::uint64_t hash{};
    bool dialing{},ready{},poses{};
    std::string name="Player", remote, state="Offline", tx,rx;
    std::vector<std::string> messages;
    std::deque<NetInput> inputs;
    double sceneTime{}; PlayerPose a{},b{};
    Clock::time_point deadline;
    ~Impl(){release(peer);release(listener);}
    void drop(std::string reason){release(peer);dialing=ready=poses=false;tx.clear();rx.clear();remote.clear();inputs.clear();state=std::move(reason);}
    bool fail(const char* reason){drop(reason);return false;}
    void queue(const std::string& line){if(tx.size()+line.size()>65536){drop("Send queue full");return;}tx+=line;tx+='\n';}
    void message(std::string text){if(messages.size()==128)messages.erase(messages.begin());messages.push_back(std::move(text));}
    void hello(){dialing=false;state="Checking map";deadline=Clock::now()+std::chrono::seconds(10);int one=1;
        setsockopt(peer,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<const char*>(&one),sizeof(one));
#ifdef SO_NOSIGPIPE
        setsockopt(peer,SOL_SOCKET,SO_NOSIGPIPE,reinterpret_cast<const char*>(&one),sizeof(one));
#endif
        queue("RAY2 "+std::to_string(hash)+" "+name);
    }
    bool line(const std::string& s){
        if(!ready){std::string prefix="RAY2 "+std::to_string(hash)+" ";
            if(s.compare(0,prefix.size(),prefix)!=0)return fail("Map or protocol mismatch");
            remote=clean(s.substr(prefix.size()),48);if(remote.empty())return false;
            ready=true;state="Connected";return true;}
        if(s.size()<3 || s[1]!=' ')return false;
        const auto body=s.substr(2);
        if(s[0]=='N'){remote=clean(body,48);return true;}
        if(s[0]=='C'){message(remote+": "+clean(body,512));return true;}
        const char* cur = body.c_str();
        if(s[0]=='I' && listener!=invalid){NetInput v;int jump=0;
            if(!scanInt(cur,v.forward)||!scanInt(cur,v.strafe)||!scanInt(cur,v.dx)||!scanInt(cur,v.dy)||!scanInt(cur,jump)||*cur!='\0'||(jump!=0 && jump!=1)||inputs.size()>=256)return false;
            v.forward=std::clamp(v.forward,-1,1);v.strafe=std::clamp(v.strafe,-1,1);v.dx=std::clamp(v.dx,-32767,32767);v.dy=std::clamp(v.dy,-32767,32767);v.jump=jump!=0;inputs.push_back(v);return true;}
        if(s[0]=='P' && listener==invalid && pose(cur,a)&&pose(cur,b)&&scanDouble(cur,sceneTime)&&std::isfinite(sceneTime)&&sceneTime>=0&&*cur=='\0'){poses=true;return true;}
        return false;
    }
};
NetSession::NetSession():p(std::make_unique<Impl>()){available();}
NetSession::~NetSession()=default;
void NetSession::disconnect(){release(p->listener);p->drop("Offline");p->port=0;p->messages.clear();}
bool NetSession::host(std::uint16_t port,std::uint64_t hash){disconnect();p->hash=hash;
    if(!available())return p->fail("Network unavailable");
    Socket s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(port);
    Length len=sizeof(addr);
    if(s==invalid || !nonblock(s) || bind(s,reinterpret_cast<sockaddr*>(&addr),sizeof(addr)) || listen(s,1) || getsockname(s,reinterpret_cast<sockaddr*>(&addr),&len)){release(s);return p->fail("Cannot listen on port");}
    p->listener=s;p->port=ntohs(addr.sin_port);p->state="Listening on "+std::to_string(p->port);
    char hostname[256]{}; addrinfo hints{}, *list=nullptr; hints.ai_family=AF_INET; hints.ai_socktype=SOCK_STREAM;
    if(gethostname(hostname,sizeof(hostname)-1)==0 && getaddrinfo(hostname,nullptr,&hints,&list)==0){
        for(auto* entry=list;entry;entry=entry->ai_next){char ip[INET_ADDRSTRLEN]{};auto* local=reinterpret_cast<const sockaddr_in*>(entry->ai_addr);
            if(inet_ntop(AF_INET,&local->sin_addr,ip,sizeof(ip)))p->message("connect "+std::string(ip)+":"+std::to_string(p->port));}
        freeaddrinfo(list);
    }
    return true;
}
bool NetSession::connect(std::string address,std::uint16_t port,std::uint64_t hash){disconnect();p->hash=hash;
    if(!available())return p->fail("Network unavailable");
    sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(port);if(address=="localhost")address="127.0.0.1";
    if(!port || inet_pton(AF_INET,address.c_str(),&addr.sin_addr)!=1)return p->fail("Expected IPv4 address and port");
    p->peer=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(p->peer==invalid || !nonblock(p->peer))return p->fail("Cannot open socket");
    if(::connect(p->peer,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))==0){p->hello();return true;}
    if(!pending())return p->fail("Connection failed");
    p->dialing=true;p->state="Connecting";p->deadline=Clock::now()+std::chrono::seconds(10);return true;
}
void NetSession::poll(){
    if(isHost() && (p->peer==invalid || p->ready)){Socket s=accept(p->listener,nullptr,nullptr);if(s!=invalid){if(p->peer!=invalid || !nonblock(s))release(s);else{p->peer=s;p->hello();}}}
    if(p->peer==invalid)return;
    if(!p->ready && Clock::now()>p->deadline){p->drop("Connection timed out");return;}
    if(p->dialing){int r=writable(p->peer);if(r==0 || (r<0 && pending()))return;int e=0;Length n=sizeof(e);
        if(r<0 || getsockopt(p->peer,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&e),&n) || e){p->drop("Connection failed");return;}p->hello();}
    if(!p->tx.empty()){
#ifdef MSG_NOSIGNAL
        constexpr int flags=MSG_NOSIGNAL;
#else
        constexpr int flags=0;
#endif
        auto n=send(p->peer,p->tx.data(),static_cast<int>(p->tx.size()),flags);
        if(n>0)p->tx.erase(0,static_cast<size_t>(n));else if(n==0 || !pending()){p->drop("Send failed");return;}}
    char buf[4096];auto n=recv(p->peer,buf,sizeof(buf),0);
    if(n<=0){if(n==0 || !pending())p->drop("Peer disconnected");return;}
    p->rx.append(buf,static_cast<size_t>(n));size_t start=0,end;
    while((end=p->rx.find('\n',start))!=std::string::npos){if(end-start>1024 || !p->line(p->rx.substr(start,end-start))){if(p->peer!=invalid)p->drop("Invalid packet");return;}start=end+1;}
    p->rx.erase(0,start);if(p->rx.size()>1024)p->drop("Packet too long");
}
bool NetSession::connected()const{return p->ready;}
bool NetSession::isHost()const{return p->listener!=invalid;}
bool NetSession::isClient()const{return !isHost() && p->peer!=invalid;}
std::uint16_t NetSession::localPort()const{return p->port;}
std::string NetSession::status()const{return p->state;}
std::vector<std::string> NetSession::players()const{if(!connected())return {p->name};return isHost()?std::vector<std::string>{p->name,p->remote}:std::vector<std::string>{p->remote,p->name};}
std::vector<std::string> NetSession::drainMessages(){return std::exchange(p->messages,{});}
void NetSession::setName(std::string name){p->name=clean(std::move(name),48);if(p->name.empty())p->name="Player";if(p->peer!=invalid && !p->dialing)p->queue("N "+p->name);}
void NetSession::say(std::string s){s=clean(std::move(s),512);if(connected()&&!s.empty()){p->queue("C "+s);if(connected())p->message(p->name+": "+s);}}
void NetSession::sendInput(const NetInput& v){if(isClient()&&connected())p->queue("I "+std::to_string(v.forward)+" "+std::to_string(v.strafe)+" "+std::to_string(v.dx)+" "+std::to_string(v.dy)+" "+(v.jump?"1":"0"));}
bool NetSession::takeInput(NetInput& v){if(p->inputs.empty())return false;v=p->inputs.front();p->inputs.pop_front();return true;}
double NetSession::sceneTime() const{return p->sceneTime;}
void NetSession::sendPoses(const PlayerPose& a,const PlayerPose& b,double seconds){if(!isHost()||!connected())return;char buf[512];const int n=std::snprintf(buf,sizeof(buf),"P %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g",a.x,a.y,a.feet,a.yaw,a.pitch,b.x,b.y,b.feet,b.yaw,b.pitch,seconds);if(n>0&&static_cast<size_t>(n)<sizeof(buf))p->queue(std::string(buf,static_cast<size_t>(n)));}
bool NetSession::takePoses(PlayerPose& a,PlayerPose& b){if(!p->poses)return false;a=p->a;b=p->b;p->poses=false;return true;}
