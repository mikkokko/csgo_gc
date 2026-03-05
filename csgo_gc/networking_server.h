#pragma once

#include "networking_shared.h"

class GCMessageWrite;

// this wrapper type is useless now, but i can't be bothered to inline it
class ClientSet
{
public:
    bool Add(uint64_t id)
    {
        auto [it, inserted] = m_set.insert(id);
        return inserted;
    }

    bool Remove(uint64_t id)
    {
        return (m_set.erase(id) == 1);
    }

    bool Has(uint64_t id)
    {
        return (m_set.find(id) != m_set.end());
    }

private:
    std::unordered_set<uint64_t> m_set;
};

class NetworkingServer
{
public:
    NetworkingServer(ISteamNetworkingMessages *networkingMessages);

    // caller need to call message->Release() because fuck you
    bool ReceiveMessage(SteamNetworkingMessage_t *&message);

    void ClientConnected(uint64_t steamId, const void *ticket, uint32_t ticketSize);
    void ClientDisconnected(uint64_t steamId);

    void SendMessage(uint64_t steamId, const void *data, uint32_t size);

private:
    ISteamNetworkingMessages *const m_networkingMessages;
    ClientSet m_clients;

    STEAM_GAMESERVER_CALLBACK(NetworkingServer,
        OnSessionRequest,
        SteamNetworkingMessagesSessionRequest_t,
        m_sessionRequest);

    STEAM_GAMESERVER_CALLBACK(NetworkingServer,
        OnSessionFailed,
        SteamNetworkingMessagesSessionFailed_t,
        m_sessionFailed);
};
