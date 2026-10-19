#include "stdafx.h"
#include "networking_server.h"
#include "gc_shared.h"

NetworkingServer::NetworkingServer(ISteamNetworkingMessages *networkingMessages, SharedGC &gc)
    : m_networkingMessages{ networkingMessages }
    , m_gc{ gc }
    , m_sessionRequest{ this, &NetworkingServer::OnSessionRequest }
    , m_sessionFailed{ this, &NetworkingServer::OnSessionFailed }
{
}

// helper for SteamNetworkingMessages::SendMessageToUser that attempts to do some kind of error handling
static void SendMessageToUser(ISteamNetworkingMessages *networkingMessages, uint64_t steamId, const void *data, uint32_t size)
{
    SteamNetworkingIdentity identity;
    identity.SetSteamID64(steamId);

    EResult result = networkingMessages->SendMessageToUser(
        identity,
        data,
        size,
        NetMessageSendFlags,
        NetMessageChannel);

    if (result != k_EResultOK)
    {
        Platform::Print("SendMessageToUser failed for {}: {}, closing session and trying again\n", steamId, static_cast<int>(result));

        networkingMessages->CloseChannelWithUser(identity, NetMessageChannel);

        result = networkingMessages->SendMessageToUser(
            identity,
            data,
            size,
            NetMessageSendFlags,
            NetMessageChannel);

        if (result != k_EResultOK)
        {
            // not much we can do in this situation i guess
            Platform::Print("SendMessageToUser failed for {}\n", steamId);
        }
    }
}

void NetworkingServer::Update()
{
    SteamNetworkingMessage_t *message;
    while (m_networkingMessages->ReceiveMessagesOnChannel(NetMessageChannel, &message, 1))
    {
        m_gc.PostToGC(GCEvent::NetMessage, message->m_identityPeer.GetSteamID64(), message->GetData(), message->GetSize());
        message->Release();
    }
}

void NetworkingServer::SendMessage(uint64_t steamId, const void *data, uint32_t size)
{
    SendMessageToUser(m_networkingMessages, steamId, data, size);
}

void NetworkingServer::CloseChannel(uint64_t steamId)
{
    SteamNetworkingIdentity identity;
    identity.SetSteamID64(steamId);
    m_networkingMessages->CloseChannelWithUser(identity, NetMessageChannel);
}

void NetworkingServer::OnSessionRequest(SteamNetworkingMessagesSessionRequest_t *param)
{
    m_gc.PostToGC(GCEvent::SessionRequest, param->m_identityRemote.GetSteamID64(), nullptr, 0);
}

void NetworkingServer::OnSessionFailed(SteamNetworkingMessagesSessionFailed_t *param)
{
    // don't do anything, rely on the auth session
    Platform::Print("OnSessionFailed: {}\n", param->m_info.m_szEndDebug);
}
