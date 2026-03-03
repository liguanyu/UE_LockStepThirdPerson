#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
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
enum class PacketType : uint8_t
{
    Hello = 0,
    Welcome = 1,
    Ready = 2,
    Start = 3,
    Input = 4,
    InputBundle = 5,
    Ping = 6,
    Pong = 7
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
    std::vector<InputFrame> frames;
};

struct Client
{
    int32_t clientId = -1;
    sockaddr_in addr {};
    bool ready = false;
};

template <typename T>
void WritePod(std::vector<uint8_t>& out, const T value)
{
    // 与 UE 客户端保持同一字节序列化方式（原样内存拷贝）。
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
    // 包布局与 UE 的 FLockstepPacketCodec 一一对应。
    std::vector<uint8_t> out;
    out.reserve(64 + packet.frames.size() * 48);

    WritePod<uint8_t>(out, static_cast<uint8_t>(packet.type));
    WritePod<int32_t>(out, packet.sessionId);
    WritePod<int32_t>(out, packet.clientId);
    WritePod<int32_t>(out, packet.startFrame);
    WritePod<int32_t>(out, packet.endFrame);
    WritePod<int32_t>(out, packet.seed);
    WritePod<int32_t>(out, packet.fixedFps);
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
        !ReadPod<int32_t>(cursor, remaining, packet.fixedFps))
    {
        return std::nullopt;
    }

    packet.type = static_cast<PacketType>(type);

    int32_t frameCount = 0;
    if (!ReadPod<int32_t>(cursor, remaining, frameCount))
    {
        return std::nullopt;
    }

    // 防御性校验，避免异常数据撑爆内存。
    if (frameCount < 0 || frameCount > 4096)
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
}

int main(int argc, char** argv)
{
    std::string host = "0.0.0.0";
    int port = 7777;
    int maxPlayers = 4;
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
        else if (arg == "--max-players" && i + 1 < argc)
        {
            maxPlayers = std::max(1, std::stoi(argv[++i]));
        }
        else if (arg == "--fps" && i + 1 < argc)
        {
            fps = std::max(1, std::stoi(argv[++i]));
        }
        else if (arg == "--help")
        {
            std::cout << "Usage: LockstepRelayServer [--host 0.0.0.0] [--port 7777] [--max-players 4] [--fps 60]\n";
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
              << " maxPlayers=" << maxPlayers
              << " fps=" << fps << "\n";

    // clientId -> 客户端会话
    std::unordered_map<int32_t, Client> clients;
    // 端点( ip:port ) -> clientId
    std::unordered_map<uint64_t, int32_t> endpointToClient;
    // frameIndex -> (playerId -> inputFrame)
    std::unordered_map<int32_t, std::unordered_map<int32_t, InputFrame>> frameBuckets;
    int32_t nextClientId = 1;
    const int32_t sessionId = 1;
    bool started = false;

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

        if (incoming.type == PacketType::Hello)
        {
            if (clients.size() >= static_cast<size_t>(maxPlayers))
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
                client.ready = false;
                clients[assignedId] = client;
                endpointToClient[endpointKey] = assignedId;

                std::cout << "Client connected id=" << assignedId << " addr=";
                PrintEndpoint(fromAddr);
                std::cout << "\n";
            }

            // HELLO -> WELCOME：分配 clientId 并下发会话参数。
            Packet welcome;
            welcome.type = PacketType::Welcome;
            welcome.sessionId = sessionId;
            welcome.clientId = assignedId;
            welcome.startFrame = 0;
            welcome.endFrame = 0;
            welcome.seed = 12345;
            welcome.fixedFps = fps;
            const auto bytesOut = Encode(welcome);
            sendto(socketFd, reinterpret_cast<const char*>(bytesOut.data()), static_cast<int>(bytesOut.size()), 0,
                   reinterpret_cast<const sockaddr*>(&fromAddr), sizeof(fromAddr));
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

        if (incoming.type == PacketType::Ready)
        {
            clientIt->second.ready = true;

            bool allReady = !clients.empty();
            for (const auto& [id, client] : clients)
            {
                if (!client.ready)
                {
                    allReady = false;
                    break;
                }
            }

            if (allReady && !started)
            {
                started = true;
                // 所有客户端 READY 后统一广播 START，避免有人提前推进。
                Packet start;
                start.type = PacketType::Start;
                start.sessionId = sessionId;
                start.fixedFps = fps;
                const auto startBytes = Encode(start);

                for (const auto& [id, client] : clients)
                {
                    sendto(socketFd, reinterpret_cast<const char*>(startBytes.data()), static_cast<int>(startBytes.size()), 0,
                           reinterpret_cast<const sockaddr*>(&client.addr), sizeof(client.addr));
                }
                std::cout << "All clients ready. START broadcast.\n";
            }
            continue;
        }

        if (incoming.type == PacketType::Input)
        {
            for (InputFrame frame : incoming.frames)
            {
                frame.playerId = clientId;
                auto& bucket = frameBuckets[frame.frameIndex];
                bucket[clientId] = frame;

                if (bucket.size() == clients.size() && !clients.empty())
                {
                    // 当某帧收齐所有玩家输入时，打包成 INPUT_BUNDLE 广播。
                    Packet bundle;
                    bundle.type = PacketType::InputBundle;
                    bundle.sessionId = sessionId;
                    bundle.startFrame = frame.frameIndex;
                    bundle.endFrame = frame.frameIndex;
                    bundle.fixedFps = fps;
                    bundle.frames.reserve(bucket.size());
                    for (const auto& [pid, frameValue] : bucket)
                    {
                        bundle.frames.push_back(frameValue);
                    }
                    std::sort(bundle.frames.begin(), bundle.frames.end(),
                              [](const InputFrame& a, const InputFrame& b) { return a.playerId < b.playerId; });

                    const auto bundleBytes = Encode(bundle);
                    for (const auto& [id, client] : clients)
                    {
                        sendto(socketFd, reinterpret_cast<const char*>(bundleBytes.data()), static_cast<int>(bundleBytes.size()), 0,
                               reinterpret_cast<const sockaddr*>(&client.addr), sizeof(client.addr));
                    }
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
            const auto pongBytes = Encode(pong);
            sendto(socketFd, reinterpret_cast<const char*>(pongBytes.data()), static_cast<int>(pongBytes.size()), 0,
                   reinterpret_cast<const sockaddr*>(&clientIt->second.addr), sizeof(clientIt->second.addr));
            continue;
        }
    }

    CloseSocket(socketFd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
