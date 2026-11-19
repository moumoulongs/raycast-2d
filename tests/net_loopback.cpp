#include "../src/net_session.h"
#include <chrono>
#include <iostream>
#include <thread>

#define CHECK(expr) do { if (!(expr)) { std::cerr << "failed: " #expr " at line " << __LINE__ << '\n'; return 1; } } while(false)
int main(){
    NetSession h,c; h.setName("Alice");c.setName("Bob");
    CHECK(h.host(0,123));CHECK(c.connect("localhost",h.localPort(),123));
    auto until=[&](auto predicate){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);do{h.poll();c.poll();if(predicate())return true;std::this_thread::sleep_for(std::chrono::milliseconds(1));}while(std::chrono::steady_clock::now()<end);return false;};
    CHECK(until([&]{return h.connected()&&c.connected();}));CHECK(h.isHost()&&c.isClient());
    CHECK(h.players()==c.players());CHECK(h.players()==(std::vector<std::string>{"Alice","Bob"}));
    h.drainMessages();
    c.setName("B");c.say("hello world");CHECK(until([&]{return h.drainMessages()==std::vector<std::string>{"B: hello world"};}));
    c.sendInput({1,-1,12,-17,true});NetInput i;CHECK(until([&]{return h.takeInput(i);}));CHECK(i.forward==1&&i.strafe==-1&&i.dx==12&&i.dy==-17&&i.jump);CHECK(!h.takeInput(i));
    PlayerPose a{1.25,2,3,4,5},b{6,7,8,9,10},x{},y{};h.sendPoses(a,b,6.25);CHECK(until([&]{return c.takePoses(x,y);}));CHECK(c.sceneTime()==6.25);CHECK(x.x==a.x&&x.y==a.y&&x.feet==a.feet&&x.yaw==a.yaw&&x.pitch==a.pitch&&y.x==b.x&&y.y==b.y&&y.feet==b.feet&&y.yaw==b.yaw&&y.pitch==b.pitch);CHECK(!c.takePoses(x,y));
    c.disconnect();CHECK(until([&]{return !h.connected();}));CHECK(h.isHost());
    CHECK(c.connect("127.0.0.1",h.localPort(),124));CHECK(until([&]{return !c.isClient();}));CHECK(!c.connected()&&!h.connected());
    CHECK(c.connect("127.0.0.1",h.localPort(),123));CHECK(until([&]{return c.connected()&&h.connected();}));
}
