#pragma once

#include <steam/isteamnetworkingmessages.h>

class ClientGC;

class NetworkingClient
{
public:
    NetworkingClient(ClientGC *clientGC);

    void Update();

    void SendMessage(uint32_t type, const google::protobuf::MessageLite &message);

private:
    STEAM_CALLBACK(NetworkingClient,
        OnSessionRequest,
        SteamNetworkingMessagesSessionRequest_t,
        m_sessionRequest);

    STEAM_CALLBACK(NetworkingClient,
        OnSessionFailed,
        SteamNetworkingMessagesSessionFailed_t,
        m_sessionFailed);

    ClientGC *const m_clientGC;
    uint64_t m_serverSteamId{};
};
