#pragma once

#include "gc_message.h"
#include "networking_server.h"

class ServerGC
{
public:
    ServerGC();
    ~ServerGC();

    void HandleMessage(uint32_t type, const void *data, uint32_t size);
    bool HasOutgoingMessages(uint32_t &size);
    bool PopOutgoingMessage(uint32_t &type, void *buffer, uint32_t bufferSize, uint32_t &size);

    void ClientConnected(uint64_t steamId);
    void ClientDisconnected(uint64_t steamId);

    void Update();

    // called from net code
    void AddOutgoingMessage(const void *data, uint32_t size);

private:
    void OnServerHello(const void *data, uint32_t size);
    void IncrementKillCountAttribute(const void *data, uint32_t size);

    std::queue<OutgoingMessage> m_outgoingMessages;
    NetworkingServer m_networking;
};
