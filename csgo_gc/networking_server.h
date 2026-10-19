#pragma once

#include "networking_shared.h"

class SharedGC;

class NetworkingServer
{
public:
    NetworkingServer(ISteamNetworkingMessages *networkingMessages, SharedGC &gc);

    void Update();

    void SendMessage(uint64_t steamId, const void *data, uint32_t size);
    void CloseChannel(uint64_t steamId);

private:
    ISteamNetworkingMessages *const m_networkingMessages;
    SharedGC &m_gc;

    STEAM_GAMESERVER_CALLBACK(NetworkingServer,
        OnSessionRequest,
        SteamNetworkingMessagesSessionRequest_t,
        m_sessionRequest);

    STEAM_GAMESERVER_CALLBACK(NetworkingServer,
        OnSessionFailed,
        SteamNetworkingMessagesSessionFailed_t,
        m_sessionFailed);
};
