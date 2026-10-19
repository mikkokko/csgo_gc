#include "stdafx.h"
#include "networking_client.h"
#include "gc_shared.h"

NetworkingClient::NetworkingClient(ISteamNetworkingMessages *networkingMessages, SharedGC &gc)
    : m_networkingMessages{ networkingMessages }
    , m_gc{ gc }
    , m_sessionRequest{ this, &NetworkingClient::OnSessionRequest }
    , m_sessionFailed{ this, &NetworkingClient::OnSessionFailed }
{
}

void NetworkingClient::Update()
{
    SteamNetworkingMessage_t *message;
    while (m_networkingMessages->ReceiveMessagesOnChannel(NetMessageChannel, &message, 1))
    {
        m_gc.PostToGC(GCEvent::NetMessage, message->m_identityPeer.GetSteamID64(), message->GetData(), message->GetSize());
        message->Release();
    }
}

void NetworkingClient::SendMessage(uint64_t steamId, const void *data, uint32_t size)
{
    // mikkotodo check return
    SteamNetworkingIdentity identity;
    identity.SetSteamID64(steamId);

    [[maybe_unused]] EResult result = m_networkingMessages->SendMessageToUser(
        identity,
        data,
        size,
        NetMessageSendFlags,
        NetMessageChannel);

    assert(result == k_EResultOK);
}

void NetworkingClient::CloseChannel(uint64_t steamId)
{
    SteamNetworkingIdentity identity;
    identity.SetSteamID64(steamId);
    m_networkingMessages->CloseChannelWithUser(identity, NetMessageChannel);
}

void NetworkingClient::OnSessionRequest(SteamNetworkingMessagesSessionRequest_t *param)
{
    if (!param->m_identityRemote.GetSteamID().BGameServerAccount())
    {
        // csgo_gc related connections come from gameservers
        return;
    }

    // accept the connection, we should receive the k_EMsgNetworkConnect message
    m_networkingMessages->AcceptSessionWithUser(param->m_identityRemote);
}

void NetworkingClient::OnSessionFailed(SteamNetworkingMessagesSessionFailed_t *param)
{
    Platform::Print("NetworkingClient::OnSessionFailed: {}\n", param->m_info.m_szEndDebug);
}
