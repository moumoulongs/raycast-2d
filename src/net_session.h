#pragma once
#include "player_pose.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct NetInput { int forward{}, strafe{}, dx{}, dy{}; bool jump{}; };

// Single-threaded; call poll each frame, including while connecting/listening.
class NetSession {
public:
    NetSession();
    ~NetSession();
    bool host(std::uint16_t port, std::uint64_t mapHash);
    // Numeric IPv4 or localhost; no blocking DNS lookup.
    bool connect(std::string address, std::uint16_t port, std::uint64_t mapHash);
    void setName(std::string name);
    void say(std::string message);
    void poll();
    void disconnect();
    bool connected() const;
    bool isHost() const;
    bool isClient() const;
    std::string status() const;
    std::vector<std::string> players() const;
    std::vector<std::string> drainMessages();
    void sendInput(const NetInput& input);
    bool takeInput(NetInput& input);
    double sceneTime() const;
    void sendPoses(const PlayerPose& host, const PlayerPose& client, double sceneTime=0);
    bool takePoses(PlayerPose& host, PlayerPose& client);
    std::uint16_t localPort() const;
private:
    struct Impl;
    std::unique_ptr<Impl> p;
};
