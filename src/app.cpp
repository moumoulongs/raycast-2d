#include "app.h"
#include "console_ui.h"
#include "net_session.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include "game_resource.h"
#include <optional>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <timeapi.h>
#endif

namespace {
#ifdef _WIN32
struct HighResPacer {
    HANDLE timer = nullptr;
    HighResPacer() {
        timeBeginPeriod(1);
#ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
        timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
#endif
    }
    ~HighResPacer() {
        if (timer) CloseHandle(timer);
        timeEndPeriod(1);
    }
    void waitUntil(std::chrono::steady_clock::time_point deadline) const {
        using Clock = std::chrono::steady_clock;
        for (;;) {
            const auto now = Clock::now();
            if (now >= deadline) return;
            const auto remUs = std::chrono::duration_cast<std::chrono::microseconds>(deadline - now).count();
            if (timer && remUs > 400) {
                LARGE_INTEGER due{};
                // Leave ~300us headroom for sub-millisecond spin finish.
                due.QuadPart = -static_cast<LONGLONG>(remUs - 300) * 10;
                if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
                    WaitForSingleObject(timer, 50);
                    continue;
                }
            } else if (!timer && remUs > 1350) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            YieldProcessor();
        }
    }
};
#else
struct HighResPacer {
    void waitUntil(std::chrono::steady_clock::time_point deadline) const {
        using Clock = std::chrono::steady_clock;
        for (;;) {
            const auto now = Clock::now();
            if (now >= deadline) return;
            if (deadline - now > std::chrono::microseconds(1350))
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            else
                std::this_thread::yield();
        }
    }
};
#endif
constexpr unsigned defaultPort=27015;
bool parsePort(const std::string& s,unsigned& port){
    auto r=std::from_chars(s.data(),s.data()+s.size(),port);
    return r.ec==std::errc{} && r.ptr==s.data()+s.size() && port>0 && port<=65535;
}
std::uint64_t hashResources(const std::filesystem::path& map, const std::filesystem::path& atlas) {
    std::uint64_t hash = 14695981039346656037ULL;
    auto add = [&](unsigned char c) { hash = (hash ^ c) * 1099511628211ULL; };
    for (const auto& path : {map, atlas}) {
        std::string data;
        if (!readGameResource(path, data)) return 0;
        std::size_t pos = 0;
        while (pos < data.size()) {
            const std::size_t nl = data.find('\n', pos);
            std::size_t end = (nl == std::string::npos) ? data.size() : nl;
            if (end > pos && data[end - 1] == '\r') --end;
            for (std::size_t i = pos; i < end; ++i) add(static_cast<unsigned char>(data[i]));
            add('\n');
            if (nl == std::string::npos) break;
            pos = nl + 1;
        }
        add(0);
    }
    return hash;
}
std::string command(NetSession& net,const std::string& line,const std::string& map,std::uint64_t hash){
    auto end=line.find_first_of(" \t"); auto op=line.substr(0,end);
    auto begin=line.find_first_not_of(" \t",end); auto arg=begin==line.npos?std::string{}:line.substr(begin);
    const std::pair<const char*,const char*> usage[]={{"host","host [port]"},{"connect","connect <IPv4>[:port]"},{"name","name <display name>"},{"say","say <message>"},{"status","status: mode, map and endpoint"},{"players","players: list room members"},{"disconnect","disconnect: leave or close room"}};
    if(op=="help") {if(arg.empty()) return "help [command] | host | connect | name | say | status | players | disconnect | clear"; for(auto u:usage)if(arg==u.first)return u.second;return "No help for: "+arg;}
    unsigned port=defaultPort;
    if(op=="host" || op=="connect") {
        if(net.isHost() || net.isClient()) return "Already in a session. Use disconnect first.";
        if(op=="host") {if(!arg.empty()&&!parsePort(arg,port))return "Usage: host [1-65535]";net.host(static_cast<std::uint16_t>(port),hash);}
        else {auto colon=arg.find(':');if(arg.empty() || colon==0 || (colon!=arg.npos&&!parsePort(arg.substr(colon+1),port)))return "Usage: connect <IPv4>[:port]";net.connect(arg.substr(0,colon),static_cast<std::uint16_t>(port),hash);} return net.status();
    }
    if(op=="name" || op=="say"){if(arg.empty())return op+" requires text";if(op=="name")net.setName(arg);else net.say(arg);return {};}
    if(op=="disconnect"){net.disconnect();return "Disconnected.";}
    if(op=="status")return net.status()+" | map: "+map;
    if(op=="players"){std::string out;for(auto& s:net.players()){if(!out.empty())out+=" | ";out+=s;}return out;}
    return "Unknown command. Type help.";
}
}
App::App(std::unique_ptr<Platform> platform, WorldMap map,std::uint64_t fingerprint,std::string mapName,
         std::vector<RealmEntry> realms, int activeRealm, int fpsCap, std::filesystem::path assetDir)
    : platform_(std::move(platform)),worldMap_(std::move(map)),camera_(worldMap_.spawnPosition(),worldMap_.spawnDirection()),
      screen_(platform_->width(),platform_->height()), fingerprint_(fingerprint),mapName_(std::move(mapName)),
      realms_(std::move(realms)), activeRealm_(activeRealm), fpsCap_(fpsCap), assetDir_(std::move(assetDir)) {}

int App::run() {
    using Clock=std::chrono::steady_clock;
    const auto frameBudget = fpsCap_ > 0 ? std::chrono::microseconds(1000000 / fpsCap_) : std::chrono::microseconds(0);
    HighResPacer pacer;
    auto animationStart=Clock::now();
    auto previous=animationStart;
    auto nextFrame=animationStart;
    NetSession net; ConsoleUi console; HudUi hud(realms_, activeRealm_); StoryRuntime story; Camera guest; NetInput held{}; bool hadPeer=false, wasClient=false;
    auto loadStoryForCurrentMap = [&] {
        story.reset();
        if (!worldMap_.storyFile().empty() && !assetDir_.empty()) {
            story.load(assetDir_ / std::filesystem::u8path(worldMap_.storyFile()));
        }
    };
    loadStoryForCurrentMap();
    std::optional<PlayerPose> other;
    auto fresh=[&]{return Camera(worldMap_.spawnPosition(),worldMap_.spawnDirection());};
    double sceneTime=0;
    double displayedFps = fpsCap_ > 0 ? static_cast<double>(fpsCap_) : 0.0;
    double fpsWindowTime = 0.0;
    int fpsWindowFrames = 0;
    double renderMsSum = 0.0, presentMsSum = 0.0;
    double displayedRenderMs = 0.0, displayedPresentMs = 0.0;
    double netSendAccum = 0.0;
    for (;;) {
        auto start=Clock::now();
        double rawDt = std::chrono::duration<double>(start-previous).count();
        double dt = std::clamp(rawDt, 0.0, 0.05);
        previous=start;
        fpsWindowTime += rawDt;
        ++fpsWindowFrames;
        if (fpsWindowTime >= 0.25) {
            displayedFps = fpsWindowFrames / fpsWindowTime;
            displayedRenderMs = renderMsSum / fpsWindowFrames;
            displayedPresentMs = presentMsSum / fpsWindowFrames;
            fpsWindowTime = 0.0;
            renderMsSum = 0.0;
            presentMsSum = 0.0;
            fpsWindowFrames = 0;
        }

        auto input=platform_->pollInput();if(input.exit)break;
        if (input.viewportWidth > 0 && input.viewportHeight > 0) {
            screen_.resize(input.viewportWidth, input.viewportHeight);
        }

        console.update(input,[&](const std::string& s){return command(net,s,mapName_,fingerprint_);});
        const HudAction hudAct = hud.update(input, dt);
        if (hudAct.quit) break;
        if (hudAct.switchRealmIndex >= 0 && hudAct.switchRealmIndex < static_cast<int>(realms_.size())) {
            const auto& realm = realms_[static_cast<size_t>(hudAct.switchRealmIndex)];
            try {
                WorldMap nextMap(realm.mapPath, realm.atlasPath);
                net.disconnect();
                other.reset();
                worldMap_ = std::move(nextMap);
                camera_ = fresh();
                mapName_ = realm.mapPath.filename().u8string();
                fingerprint_ = hashResources(realm.mapPath, realm.atlasPath);
                activeRealm_ = hudAct.switchRealmIndex;
                loadStoryForCurrentMap();
                animationStart = Clock::now();
                sceneTime = 0;
            } catch (const std::exception& e) {
                console.log(std::string("Failed to load realm: ") + e.what());
            }
        }

        net.poll();for(const auto& text:net.drainMessages())console.log(text);
        const bool peer=net.connected();
        if(!peer || !hadPeer) {other.reset();held={};netSendAccum=0.0;}
        if(wasClient&&!net.isClient()) camera_=fresh();
        if (!input.consoleActive && !hud.blocksMovement()) {
            story.update(input, worldMap_, camera_.pose(), dt);
        } else if (input.toggleLang) {
            story.toggleLanguage();
        }
        NetInput mine{};
        if(!input.consoleConsumed && !input.consoleActive && !hud.blocksMovement() && !story.blocksMovement()) {
            mine={input.move,input.strafe,input.mouseX,input.mouseY,input.jump};
        }
        if(!(net.isClient() && peer)){sceneTime=std::chrono::duration<double>(start-animationStart).count();worldMap_.animate(sceneTime);}
        netSendAccum += dt;
        const bool netTick = netSendAccum >= (1.0 / 60.0);
        if (netTick) netSendAccum = std::fmod(netSendAccum, 1.0 / 60.0);
        if(net.isClient() && peer){
            if (netTick || mine.jump || mine.dx || mine.dy) net.sendInput(mine);
            PlayerPose host{},self{};
            if(net.takePoses(host,self)){auto valid=[&](const PlayerPose& p){return std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>=0&&p.y>=0&&p.x<512&&p.y<512&& !worldMap_.isWall(int(p.x),int(p.y))&&std::isfinite(p.feet)&&std::abs(p.feet)<64&&std::isfinite(p.yaw)&&std::isfinite(p.pitch);};if(valid(host)&&valid(self)){sceneTime=net.sceneTime();worldMap_.animate(sceneTime);camera_.applyPose(self);other=host;}}
        }else{
            camera_.look(mine.dx,mine.dy);camera_.advance(worldMap_,dt,mine.forward,mine.jump,mine.strafe);
            if(net.isHost() && peer){
                if(!hadPeer){guest=fresh();guest.advance(worldMap_,0,0,false);}
                NetInput next{};while(net.takeInput(next)){held.forward=next.forward;held.strafe=next.strafe;held.jump|=next.jump;guest.look(next.dx,next.dy);}
                guest.advance(worldMap_,dt,held.forward,held.jump,held.strafe);held.jump=false;other=guest.pose();
                if (netTick) net.sendPoses(camera_.pose(),*other,sceneTime);
            }
        }
        wasClient=net.isClient();hadPeer=peer;
        const auto tRender0 = Clock::now();
        screen_.clear();
        camera_.render(worldMap_,screen_,other?&*other:nullptr,sceneTime,story.horizonSway(sceneTime));
        if(input.consoleActive) console.draw(screen_);
        else hud.draw(screen_, worldMap_, camera_.pose(), mapName_, sceneTime, displayedFps, input.mouseLocked, peer, displayedRenderMs, displayedPresentMs, &story);
        const auto tPresent0 = Clock::now();
        platform_->present(screen_);
        const auto tPresent1 = Clock::now();
        renderMsSum += std::chrono::duration<double, std::milli>(tPresent0 - tRender0).count();
        presentMsSum += std::chrono::duration<double, std::milli>(tPresent1 - tPresent0).count();

        if (fpsCap_ > 0) {
            nextFrame += frameBudget;
            if (tPresent1 > nextFrame) nextFrame = tPresent1;
            else pacer.waitUntil(nextFrame);
        }
    }
    return 0;
}
