#pragma once

#include "networking_shared.h"

class SharedGC;

class NetworkingClient
{
public:
    NetworkingClient(ISteamNetworkingMessages *networkingMessages, SharedGC &gc);

    void Update();

    void SendMessage(uint64_t steamId, const void *data, uint32_t size);
    void CloseChannel(uint64_t steamId);

private:
    ISteamNetworkingMessages *const m_networkingMessages;
    SharedGC &m_gc;

    STEAM_CALLBACK(NetworkingClient,
        OnSessionRequest,
        SteamNetworkingMessagesSessionRequest_t,
        m_sessionRequest);

    STEAM_CALLBACK(NetworkingClient,
        OnSessionFailed,
        SteamNetworkingMessagesSessionFailed_t,
        m_sessionFailed);
};
