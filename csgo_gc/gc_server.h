#pragma once

#include "gc_shared.h"

class GCMessageRead;

struct GCClientState
{
    GCClientState(const std::vector<uint8_t> &ticket)
        : authTicket{ ticket }
    {
    }

    std::vector<uint8_t> authTicket;
    bool hasSOCache{};
    int connectSendCount{};
    std::chrono::steady_clock::time_point lastConnectSend;
};

class ServerGC final : public SharedGC
{
public:
    ServerGC();
    ~ServerGC();

private:
    void HandleEvent(GCEvent type, uint64_t id, const std::vector<uint8_t> &buffer) override;

    // event handlers
    void HandleMessage(uint32_t type, const void *data, uint32_t size);
    void HandleNetMessage(uint64_t steamId, const void *data, uint32_t size);
    void HandleClientSOCacheUnsubscribe(uint64_t steamId);
    void ClientConnected(uint64_t steamId, const std::vector<uint8_t> &ticket);
    void ClientDisconnected(uint64_t steamId);
    void HandleSessionRequest(uint64_t steamId);
    void UpdateConnections();

    void SendConnectMessage(uint64_t steamId, GCClientState &state);
    void SendMessageToClient(uint64_t steamId, std::vector<uint8_t> &&buffer);
    void SendServerWelcome();
    void IncrementKillCountAttribute(GCMessageRead &messageRead);

    bool m_sentWelcome{};
    std::unordered_map<uint64_t, GCClientState> m_clients;
};
