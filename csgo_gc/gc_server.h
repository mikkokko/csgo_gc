#pragma once

#include "gc_shared.h"
#include "networking_server.h"

class ServerGC final : public SharedGC
{
public:
    ServerGC(ISteamNetworkingMessages *networkingMessages);
    ~ServerGC();

    void HandleMessage(uint32_t type, const void *data, uint32_t size);

    void ClientConnected(uint64_t steamId, const void *ticket, uint32_t ticketSize);
    void ClientDisconnected(uint64_t steamId);

    void Update();

    // used to be called by the net code, still called directly by the tests so keep it public
    void HandleNetMessage(uint64_t steamId, const void *data, uint32_t size);

private:
    void OnServerHello(GCMessageRead &messageRead);
    void IncrementKillCountAttribute(GCMessageRead &messageRead);

    // don't run networking until we've received the hello and sent the welcome
    // otherwise we might receive the local client's socache before that and it'll
    // get wiped after the welcome is received
    bool m_receivedHello{};

    NetworkingServer m_networking;
};
