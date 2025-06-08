#pragma once

#include "networking_shared.h"

class ServerGC;
class GCMessageWrite;

class NetworkingServer
{
public:
    NetworkingServer(ServerGC *serverGC, ISteamNetworkingMessages *networkingMessages);

    void Update();

    void ClientConnected(uint64_t steamId, const void *ticket, uint32_t ticketSize);
    void ClientDisconnected(uint64_t steamId);

    void SendMessage(uint64_t steamId, const GCMessageWrite &message);

private:
    ServerGC *const m_serverGC;
    ISteamNetworkingMessages *const m_networkingMessages;
    std::unordered_set<uint64_t> m_clients;

    STEAM_GAMESERVER_CALLBACK(NetworkingServer,
        OnSessionRequest,
        SteamNetworkingMessagesSessionRequest_t,
        m_sessionRequest);

    STEAM_GAMESERVER_CALLBACK(NetworkingServer,
        OnSessionFailed,
        SteamNetworkingMessagesSessionFailed_t,
        m_sessionFailed);
};
