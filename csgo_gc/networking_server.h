#pragma once

#include <steam/isteamnetworkingmessages.h>

class ServerGC;

struct ClientInfo
{
    bool cacheSubscribed{};
};

class NetworkingServer
{
public:
    NetworkingServer(ServerGC *serverGC);

    void Update();

    void ClientConnected(uint64_t steamId);
    void ClientDisconnected(uint64_t steamId);

private:
    STEAM_GAMESERVER_CALLBACK(NetworkingServer,
        OnSessionRequest,
        SteamNetworkingMessagesSessionRequest_t,
        m_sessionRequest);

    STEAM_GAMESERVER_CALLBACK(NetworkingServer,
        OnSessionFailed,
        SteamNetworkingMessagesSessionFailed_t,
        m_sessionFailed);

    ServerGC *const m_serverGC;
    std::unordered_map<uint64_t, ClientInfo> m_clients;
};
