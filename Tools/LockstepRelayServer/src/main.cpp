#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

namespace
{
// 直接修改这里控制一局的目标玩家数。
// 例如：
// - 1 表示单人局，1 人 ready 后即可开始
// - 4 表示四人局，必须 4 人全部加入并 ready 后才开始
constexpr int kRequiredPlayers = 1;
constexpr bool kLogInputFrames = false;

enum class PacketType : uint8_t
{
    Hello = 0,
    Welcome = 1,
    JoinRequest = 2,
    JoinAccept = 3,
    PlayerSpawn = 4,
    PlayerSpawnAck = 5,
    Ready = 6,
    Start = 7,
    Input = 8,
    InputBundle = 9,
    RoomClosed = 10,
    Ping = 11,
    Pong = 12
};

const char* ToString(const PacketType type)
{
    switch (type)
    {
    case PacketType::Hello:
        return "Hello";
    case PacketType::Welcome:
        return "Welcome";
    case PacketType::JoinRequest:
        return "JoinRequest";
    case PacketType::JoinAccept:
        return "JoinAccept";
    case PacketType::PlayerSpawn:
        return "PlayerSpawn";
    case PacketType::PlayerSpawnAck:
        return "PlayerSpawnAck";
    case PacketType::Ready:
        return "Ready";
    case PacketType::Start:
        return "Start";
    case PacketType::Input:
        return "Input";
    case PacketType::InputBundle:
        return "InputBundle";
    case PacketType::RoomClosed:
        return "RoomClosed";
    case PacketType::Ping:
        return "Ping";
    case PacketType::Pong:
        return "Pong";
    default:
        return "Unknown";
    }
}

struct PlayerDesc
{
    int32_t playerId = -1;
    int32_t pawnTypeId = 0;
    float spawnX = 0.0f;
    float spawnY = 0.0f;
    float spawnZ = 0.0f;
    float roll = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;
};

struct InputFrame
{
    int32_t frameIndex = 0;
    int32_t playerId = -1;
    float moveX = 0.0f;
    float moveY = 0.0f;
    float lookX = 0.0f;
    float lookY = 0.0f;
    bool jumpPressed = false;
    int32_t actionBits = 0;
    int64_t timestamp = 0;
};

struct Packet
{
    PacketType type = PacketType::Hello;
    int32_t sessionId = 0;
    int32_t clientId = -1;
    int32_t startFrame = 0;
    int32_t endFrame = 0;
    int32_t seed = 0;
    int32_t fixedFps = 60;
    int32_t resultCode = 0;
    int32_t rosterVersion = 0;
    std::vector<PlayerDesc> players;
    std::vector<InputFrame> frames;
};

struct Client
{
    int32_t clientId = -1;
    int32_t playerId = -1;
    sockaddr_in addr {};
    bool joined = false;
    bool ready = false;
    std::unordered_set<int32_t> ackedPlayers;
};

template <typename T>
void WritePod(std::vector<uint8_t>& out, const T value)
{
    const auto* ptr = reinterpret_cast<const uint8_t*>(&value);
    out.insert(out.end(), ptr, ptr + sizeof(T));
}

template <typename T>
bool ReadPod(const uint8_t*& cursor, int& remaining, T& out)
{
    if (remaining < static_cast<int>(sizeof(T)))
    {
        return false;
    }
    std::memcpy(&out, cursor, sizeof(T));
    cursor += sizeof(T);
    remaining -= static_cast<int>(sizeof(T));
    return true;
}

std::vector<uint8_t> Encode(const Packet& packet)
{
    std::vector<uint8_t> out;
    out.reserve(96 + packet.players.size() * 40 + packet.frames.size() * 48);

    WritePod<uint8_t>(out, static_cast<uint8_t>(packet.type));
    WritePod<int32_t>(out, packet.sessionId);
    WritePod<int32_t>(out, packet.clientId);
    WritePod<int32_t>(out, packet.startFrame);
    WritePod<int32_t>(out, packet.endFrame);
    WritePod<int32_t>(out, packet.seed);
    WritePod<int32_t>(out, packet.fixedFps);
    WritePod<int32_t>(out, packet.resultCode);
    WritePod<int32_t>(out, packet.rosterVersion);

    WritePod<int32_t>(out, static_cast<int32_t>(packet.players.size()));
    for (const PlayerDesc& player : packet.players)
    {
        WritePod<int32_t>(out, player.playerId);
        WritePod<int32_t>(out, player.pawnTypeId);
        WritePod<float>(out, player.spawnX);
        WritePod<float>(out, player.spawnY);
        WritePod<float>(out, player.spawnZ);
        WritePod<float>(out, player.roll);
        WritePod<float>(out, player.pitch);
        WritePod<float>(out, player.yaw);
    }

    WritePod<int32_t>(out, static_cast<int32_t>(packet.frames.size()));
    for (const InputFrame& frame : packet.frames)
    {
        WritePod<int32_t>(out, frame.frameIndex);
        WritePod<int32_t>(out, frame.playerId);
        WritePod<float>(out, frame.moveX);
        WritePod<float>(out, frame.moveY);
        WritePod<float>(out, frame.lookX);
        WritePod<float>(out, frame.lookY);
        WritePod<uint8_t>(out, frame.jumpPressed ? 1 : 0);
        WritePod<int32_t>(out, frame.actionBits);
        WritePod<int64_t>(out, frame.timestamp);
    }

    return out;
}

std::optional<Packet> Decode(const uint8_t* data, const int length)
{
    if (data == nullptr || length <= 0)
    {
        return std::nullopt;
    }

    Packet packet;
    const uint8_t* cursor = data;
    int remaining = length;

    uint8_t type = 0;
    if (!ReadPod<uint8_t>(cursor, remaining, type) ||
        !ReadPod<int32_t>(cursor, remaining, packet.sessionId) ||
        !ReadPod<int32_t>(cursor, remaining, packet.clientId) ||
        !ReadPod<int32_t>(cursor, remaining, packet.startFrame) ||
        !ReadPod<int32_t>(cursor, remaining, packet.endFrame) ||
        !ReadPod<int32_t>(cursor, remaining, packet.seed) ||
        !ReadPod<int32_t>(cursor, remaining, packet.fixedFps) ||
        !ReadPod<int32_t>(cursor, remaining, packet.resultCode) ||
        !ReadPod<int32_t>(cursor, remaining, packet.rosterVersion))
    {
        return std::nullopt;
    }

    packet.type = static_cast<PacketType>(type);

    int32_t playerCount = 0;
    if (!ReadPod<int32_t>(cursor, remaining, playerCount) || playerCount < 0 || playerCount > 256)
    {
        return std::nullopt;
    }

    packet.players.reserve(static_cast<size_t>(playerCount));
    for (int i = 0; i < playerCount; ++i)
    {
        PlayerDesc player;
        if (!ReadPod<int32_t>(cursor, remaining, player.playerId) ||
            !ReadPod<int32_t>(cursor, remaining, player.pawnTypeId) ||
            !ReadPod<float>(cursor, remaining, player.spawnX) ||
            !ReadPod<float>(cursor, remaining, player.spawnY) ||
            !ReadPod<float>(cursor, remaining, player.spawnZ) ||
            !ReadPod<float>(cursor, remaining, player.roll) ||
            !ReadPod<float>(cursor, remaining, player.pitch) ||
            !ReadPod<float>(cursor, remaining, player.yaw))
        {
            return std::nullopt;
        }
        packet.players.push_back(player);
    }

    int32_t frameCount = 0;
    if (!ReadPod<int32_t>(cursor, remaining, frameCount) || frameCount < 0 || frameCount > 4096)
    {
        return std::nullopt;
    }

    packet.frames.reserve(static_cast<size_t>(frameCount));
    for (int i = 0; i < frameCount; ++i)
    {
        InputFrame frame;
        uint8_t jump = 0;
        if (!ReadPod<int32_t>(cursor, remaining, frame.frameIndex) ||
            !ReadPod<int32_t>(cursor, remaining, frame.playerId) ||
            !ReadPod<float>(cursor, remaining, frame.moveX) ||
            !ReadPod<float>(cursor, remaining, frame.moveY) ||
            !ReadPod<float>(cursor, remaining, frame.lookX) ||
            !ReadPod<float>(cursor, remaining, frame.lookY) ||
            !ReadPod<uint8_t>(cursor, remaining, jump) ||
            !ReadPod<int32_t>(cursor, remaining, frame.actionBits) ||
            !ReadPod<int64_t>(cursor, remaining, frame.timestamp))
        {
            return std::nullopt;
        }
        frame.jumpPressed = jump != 0;
        packet.frames.push_back(frame);
    }

    return packet;
}

void CloseSocket(const SocketHandle socket)
{
    if (socket == kInvalidSocket)
    {
        return;
    }
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

void PrintEndpoint(const sockaddr_in& addr)
{
    char buffer[64] = {0};
    inet_ntop(AF_INET, &addr.sin_addr, buffer, sizeof(buffer));
    std::cout << buffer << ":" << ntohs(addr.sin_port);
}

void SendPacketTo(SocketHandle socketFd, const Packet& packet, const sockaddr_in& addr)
{
    if (packet.type != PacketType::InputBundle)
    {
        std::cout << "send packet type " << ToString(packet.type)
            << " (" << static_cast<int>(packet.type) << ") to ";
        PrintEndpoint(addr);
        std::cout << std::endl;
    }

    const auto bytes = Encode(packet);
    sendto(socketFd,
           reinterpret_cast<const char*>(bytes.data()),
           static_cast<int>(bytes.size()),
           0,
           reinterpret_cast<const sockaddr*>(&addr),
           sizeof(addr));
}

void PrintInputFrame(const int32_t clientId, const InputFrame& frame)
{
    std::cout << "Input frame received"
              << " clientId=" << clientId
              << " playerId=" << frame.playerId
              << " frameIndex=" << frame.frameIndex
              << " moveX=" << frame.moveX
              << " moveY=" << frame.moveY
              << " lookX=" << frame.lookX
              << " lookY=" << frame.lookY
              << " jumpPressed=" << (frame.jumpPressed ? 1 : 0)
              << " actionBits=" << frame.actionBits
              << " timestamp=" << frame.timestamp
              << "\n";
}
}

int main(int argc, char** argv)
{
    std::string host = "0.0.0.0";
    int port = 7777;
    int fps = 60;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg(argv[i]);
        if (arg == "--host" && i + 1 < argc)
        {
            host = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            port = std::stoi(argv[++i]);
        }
        else if (arg == "--fps" && i + 1 < argc)
        {
            fps = std::max(1, std::stoi(argv[++i]));
        }
        else if (arg == "--help")
        {
            std::cout << "Usage: LockstepRelayServer [--host 0.0.0.0] [--port 7777] [--fps 60]\n";
            return 0;
        }
    }

#ifdef _WIN32
    WSADATA wsaData {};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    SocketHandle socketFd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketFd == kInvalidSocket)
    {
        std::cerr << "Failed to create UDP socket\n";
        return 1;
    }

    sockaddr_in bindAddr {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &bindAddr.sin_addr) <= 0)
    {
        std::cerr << "Invalid host: " << host << "\n";
        CloseSocket(socketFd);
        return 1;
    }

    if (bind(socketFd, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) != 0)
    {
        std::cerr << "Failed to bind UDP socket on " << host << ":" << port << "\n";
        CloseSocket(socketFd);
        return 1;
    }

    std::cout << "Lockstep relay started on " << host << ":" << port
              << " requiredPlayers=" << kRequiredPlayers
              << " fps=" << fps
              << " logInput=" << (kLogInputFrames ? "on" : "off") << "\n";

    std::unordered_map<int32_t, Client> clients;
    std::unordered_map<uint64_t, int32_t> endpointToClient;
    std::unordered_map<int32_t, PlayerDesc> roster;
    std::unordered_map<int32_t, std::unordered_map<int32_t, InputFrame>> frameBuckets;

    int32_t nextClientId = 1;
    int32_t nextPlayerId = 1;
    int32_t rosterVersion = 0;
    const int32_t sessionId = 1;
    bool started = false;

    auto broadcastToJoined = [&](const Packet& packet)
    {
        for (const auto& [id, client] : clients)
        {
            if (client.joined)
            {
                SendPacketTo(socketFd, packet, client.addr);
            }
        }
    };

    auto buildPlayerSpawnPacket = [&]()
    {
        Packet spawnPacket;
        spawnPacket.type = PacketType::PlayerSpawn;
        spawnPacket.sessionId = sessionId;
        spawnPacket.fixedFps = fps;
        spawnPacket.rosterVersion = rosterVersion;
        spawnPacket.players.reserve(roster.size());
        for (const auto& [playerId, playerDesc] : roster)
        {
            spawnPacket.players.push_back(playerDesc);
        }
        std::sort(spawnPacket.players.begin(), spawnPacket.players.end(), [](const PlayerDesc& a, const PlayerDesc& b)
        {
            return a.playerId < b.playerId;
        });
        return spawnPacket;
    };

    auto canStartMatch = [&]()
    {
        // 只有达到目标人数时才允许开局，避免“四人房一人 ready 就开局”。
        if (static_cast<int>(roster.size()) != kRequiredPlayers)
        {
            return false;
        }

        for (const auto& [id, client] : clients)
        {
            if (!client.joined)
            {
                continue;
            }

            if (!client.ready)
            {
                return false;
            }

            for (const auto& [playerId, playerDesc] : roster)
            {
                if (!client.ackedPlayers.count(playerId))
                {
                    return false;
                }
            }
        }

        return true;
    };

    std::array<uint8_t, 65535> buffer {};
    while (true)
    {
        sockaddr_in fromAddr {};
#ifdef _WIN32
        int fromLen = sizeof(fromAddr);
#else
        socklen_t fromLen = sizeof(fromAddr);
#endif
        const int bytes = recvfrom(
            socketFd,
            reinterpret_cast<char*>(buffer.data()),
            static_cast<int>(buffer.size()),
            0,
            reinterpret_cast<sockaddr*>(&fromAddr),
            &fromLen);

        if (bytes <= 0)
        {
            continue;
        }

        const auto decoded = Decode(buffer.data(), bytes);
        if (!decoded.has_value())
        {
            continue;
        }

        Packet incoming = decoded.value();
        const uint64_t endpointKey = (static_cast<uint64_t>(fromAddr.sin_addr.s_addr) << 16u) | fromAddr.sin_port;

        if (incoming.type != PacketType::Input)
        {
            std::cout << "recv incoming type " << ToString(incoming.type)
                << " (" << static_cast<int>(incoming.type) << ")" << std::endl;
        }

        if (incoming.type == PacketType::Hello)
        {
            if (clients.size() >= static_cast<size_t>(kRequiredPlayers))
            {
                continue;
            }

            int32_t assignedId = -1;
            auto endpointIt = endpointToClient.find(endpointKey);
            if (endpointIt != endpointToClient.end())
            {
                assignedId = endpointIt->second;
            }
            else
            {
                assignedId = nextClientId++;
                Client client;
                client.clientId = assignedId;
                client.addr = fromAddr;
                clients[assignedId] = client;
                endpointToClient[endpointKey] = assignedId;
                std::cout << "Client connected id=" << assignedId << " addr=";
                PrintEndpoint(fromAddr);
                std::cout << "\n";
            }

            Packet welcome;
            welcome.type = PacketType::Welcome;
            welcome.sessionId = sessionId;
            welcome.clientId = assignedId;
            welcome.startFrame = 0;
            welcome.endFrame = 0;
            welcome.seed = 12345;
            welcome.fixedFps = fps;
            welcome.rosterVersion = rosterVersion;
            SendPacketTo(socketFd, welcome, fromAddr);
            continue;
        }

        auto endpointIt = endpointToClient.find(endpointKey);
        if (endpointIt == endpointToClient.end())
        {
            continue;
        }

        const int32_t clientId = endpointIt->second;
        auto clientIt = clients.find(clientId);
        if (clientIt == clients.end())
        {
            continue;
        }

        Client& client = clientIt->second;

        if (incoming.type == PacketType::JoinRequest)
        {
            Packet joinAccept;
            joinAccept.type = PacketType::JoinAccept;
            joinAccept.sessionId = sessionId;
            joinAccept.clientId = clientId;
            joinAccept.fixedFps = fps;
            joinAccept.rosterVersion = rosterVersion;

            if (started)
            {
                joinAccept.resultCode = 1; // 游戏已开始
                SendPacketTo(socketFd, joinAccept, client.addr);
                continue;
            }

            if (!client.joined && static_cast<int>(roster.size()) >= kRequiredPlayers)
            {
                joinAccept.resultCode = 2; // 房间已满
                SendPacketTo(socketFd, joinAccept, client.addr);
                continue;
            }

            joinAccept.resultCode = 0;
            if (!client.joined)
            {
                client.joined = true;
                client.ready = false;
                client.ackedPlayers.clear();

                client.playerId = nextPlayerId++;
                PlayerDesc player;
                player.playerId = client.playerId;
                player.pawnTypeId = 0;
                player.spawnX = 300.0f * static_cast<float>(client.playerId - 1);
                player.spawnY = 0.0f;
                player.spawnZ = 100.0f;
                player.roll = 0.0f;
                player.pitch = 0.0f;
                player.yaw = 0.0f;
                roster[player.playerId] = player;
                rosterVersion += 1;
                joinAccept.rosterVersion = rosterVersion;
            }

            SendPacketTo(socketFd, joinAccept, client.addr);
            const Packet spawnPacket = buildPlayerSpawnPacket();
            broadcastToJoined(spawnPacket);
            continue;
        }

        if (incoming.type == PacketType::PlayerSpawnAck)
        {
            for (const PlayerDesc& player : incoming.players)
            {
                if (roster.find(player.playerId) != roster.end())
                {
                    client.ackedPlayers.insert(player.playerId);
                }
            }
            continue;
        }

        if (incoming.type == PacketType::Ready)
        {
            if (!client.joined || started)
            {
                continue;
            }

            client.ready = true;
            if (canStartMatch())
            {
                started = true;
                Packet start;
                start.type = PacketType::Start;
                start.sessionId = sessionId;
                start.fixedFps = fps;
                start.rosterVersion = rosterVersion;
                broadcastToJoined(start);
                std::cout << "All players ready. START broadcast. roster=" << roster.size() << "\n";
            }
            continue;
        }

        if (incoming.type == PacketType::Input)
        {
            if (!started || !client.joined || client.playerId < 0)
            {
                continue;
            }

            for (InputFrame frame : incoming.frames)
            {
                frame.playerId = client.playerId;
                if (kLogInputFrames)
                {
                    PrintInputFrame(clientId, frame);
                }
                auto& bucket = frameBuckets[frame.frameIndex];
                bucket[client.playerId] = frame;

                if (bucket.size() == roster.size() && !roster.empty())
                {
                    Packet bundle;
                    bundle.type = PacketType::InputBundle;
                    bundle.sessionId = sessionId;
                    bundle.startFrame = frame.frameIndex;
                    bundle.endFrame = frame.frameIndex;
                    bundle.fixedFps = fps;
                    bundle.rosterVersion = rosterVersion;
                    bundle.frames.reserve(bucket.size());
                    for (const auto& [pid, frameValue] : bucket)
                    {
                        bundle.frames.push_back(frameValue);
                    }
                    std::sort(bundle.frames.begin(), bundle.frames.end(), [](const InputFrame& a, const InputFrame& b)
                    {
                        return a.playerId < b.playerId;
                    });

                    broadcastToJoined(bundle);
                    frameBuckets.erase(frame.frameIndex);
                }
            }
            continue;
        }

        if (incoming.type == PacketType::Ping)
        {
            Packet pong;
            pong.type = PacketType::Pong;
            pong.sessionId = sessionId;
            pong.clientId = clientId;
            SendPacketTo(socketFd, pong, client.addr);
            continue;
        }
    }

    CloseSocket(socketFd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
