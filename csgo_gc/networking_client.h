#pragma once

#include "networking_shared.h"

class ClientGC;
class GCMessageRead;
class GCMessageWrite;

struct AuthTicket
{
    uint64_t steamId{}; // gameserver
    std::vector<uint8_t> buffer;
};

class NetworkingClient
{
public:
    NetworkingClient(ClientGC *clientGC, ISteamNetworkingMessages *networkingMessages);

    void Update();

    void SendMessage(const GCMessageWrite &message);

    // for gameserver validation
    void SetAuthTicket(uint32_t handle, const void *data, uint32_t size);
    void ClearAuthTicket(uint32_t handle);

private:
    // return false if it wasn't handled, in which case we pass it to m_clientGC
    bool HandleMessage(uint64_t steamId, GCMessageRead &message);

    ClientGC *const m_clientGC;
    ISteamNetworkingMessages *const m_networkingMessages;
    uint64_t m_serverSteamId{};

    std::unordered_map<uint32_t, AuthTicket> m_tickets;

    STEAM_CALLBACK(NetworkingClient,
        OnSessionRequest,
        SteamNetworkingMessagesSessionRequest_t,
        m_sessionRequest);

    STEAM_CALLBACK(NetworkingClient,
        OnSessionFailed,
        SteamNetworkingMessagesSessionFailed_t,
        m_sessionFailed);
};
