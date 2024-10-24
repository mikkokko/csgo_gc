#pragma once

#include "networking_shared.h"

class ServerGC;
class GCMessageWrite;

struct ClientInfo
{
    // nothing here
    int unused;
};

class NetworkingServer
{
public:
    NetworkingServer(ServerGC *serverGC);

    void Update();

    void ClientConnected(uint64_t steamId, const void *ticket, uint32_t ticketSize);
    void ClientDisconnected(uint64_t steamId);

    void SendMessage(uint64_t steamId, const GCMessageWrite &message);

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
