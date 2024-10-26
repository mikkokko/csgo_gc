#include "stdafx.h"
#include "networking_server.h"
#include "gc_server.h"

NetworkingServer::NetworkingServer(ServerGC *serverGC)
    : m_serverGC{ serverGC }
    , m_sessionRequest{ this, &NetworkingServer::OnSessionRequest }
    , m_sessionFailed{ this, &NetworkingServer::OnSessionFailed }
{
}

void NetworkingServer::Update()
{
    SteamNetworkingMessage_t *message;
    while (SteamGameServerNetworkingMessages()->ReceiveMessagesOnChannel(NetMessageChannel, &message, 1))
    {
        uint64_t steamId = message->m_identityPeer.GetSteamID64();

        // see if we have a session
        auto it = m_clients.find(steamId);
        if (it == m_clients.end())
        {
            Platform::Print("NetworkingServer: ignored message from %llu (no session)\n", steamId);
            message->Release();
            continue;
        }

        m_serverGC->HandleNetMessage(message->GetData(), message->GetSize());

        message->Release();
    }
}

void NetworkingServer::ClientConnected(uint64_t steamId, const void *ticket, uint32_t ticketSize)
{
    auto result = m_clients.insert(steamId);
    if (!result.second)
    {
        Platform::Print("got ClientConnected for %llu but they're already on the list! ignoring\n", steamId);
        return;
    }

    // send a message, if the client has csgo_gc installed they will
    // reply with their so cache and we'll add them to our list
    SteamNetworkingIdentity identity;
    identity.SetSteamID64(steamId);

    GCMessageWrite messageWrite{ k_EMsgNetworkConnect };
    messageWrite.WriteUint32(ticketSize);
    messageWrite.WriteData(ticket, ticketSize);

    [[maybe_unused]] EResult sendResult = SteamGameServerNetworkingMessages()->SendMessageToUser(
        identity,
        messageWrite.Data(),
        messageWrite.Size(),
        NetMessageSendFlags,
        NetMessageChannel);

    assert(sendResult == k_EResultOK);
}

void NetworkingServer::ClientDisconnected(uint64_t steamId)
{
    auto it = m_clients.find(steamId);
    if (it == m_clients.end())
    {
        Platform::Print("got ClientDisconnected for %llu but they're not on the list! ignoring\n", steamId);
        return;
    }

    m_clients.erase(it);

    SteamNetworkingIdentity identity;
    identity.SetSteamID64(steamId);
    SteamGameServerNetworkingMessages()->CloseChannelWithUser(identity, NetMessageChannel);
}

void NetworkingServer::SendMessage(uint64_t steamId, const GCMessageWrite &message)
{
    auto it = m_clients.find(steamId);
    if (it == m_clients.end())
    {
        Platform::Print("No csgo_gc session with %llu, not sending message!!!\n");
        return;
    }

    // mikkotodo check return
    SteamNetworkingIdentity identity;
    identity.SetSteamID64(steamId);

    [[maybe_unused]] EResult result = SteamGameServerNetworkingMessages()->SendMessageToUser(
        identity,
        message.Data(),
        message.Size(),
        NetMessageSendFlags,
        NetMessageChannel);

    assert(result == k_EResultOK);
}

void NetworkingServer::OnSessionRequest(SteamNetworkingMessagesSessionRequest_t *param)
{
    uint64_t steamId = param->m_identityRemote.GetSteamID64();

    auto it = m_clients.find(steamId);
    if (it == m_clients.end())
    {
        Platform::Print("%llu sent a session request, we don't have a csgo_gc session, ignoring...\n");
        return;
    }

    Platform::Print("%llu sent a session request, we were playing GC with them so accept\n");

    if (!SteamGameServerNetworkingMessages()->AcceptSessionWithUser(param->m_identityRemote))
    {
        Platform::Print("AcceptSessionWithUser with %llu failed???\n",
            param->m_identityRemote.GetSteamID64());
    }
}

void NetworkingServer::OnSessionFailed(SteamNetworkingMessagesSessionFailed_t *param)
{
    // don't do anything, rely on the auth session
    Platform::Print("OnSessionFailed: %s\n", param->m_info.m_szEndDebug);
}
