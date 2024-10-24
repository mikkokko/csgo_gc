#include "stdafx.h"
#include "networking_client.h"
#include "gc_client.h"

NetworkingClient::NetworkingClient(ClientGC *clientGC)
    : m_clientGC{ clientGC }
    , m_sessionRequest{ this, &NetworkingClient::OnSessionRequest }
    , m_sessionFailed{ this, &NetworkingClient::OnSessionFailed }
{
}

void NetworkingClient::Update()
{
    SteamNetworkingMessage_t *message;
    while (SteamNetworkingMessages()->ReceiveMessagesOnChannel(NetMessageChannel, &message, 1))
    {
        uint64_t steamId = message->m_identityPeer.GetSteamID64();

        // pass 0 as type so it gets parsed from the message
        GCMessageRead messageRead{ 0, message->GetData(), message->GetSize() };
        if (!messageRead.IsValid())
        {
            assert(false);
            message->Release();
            continue;
        }

        if (HandleMessage(steamId, messageRead))
        {
            // that was an internal message
            message->Release();
            continue;
        }

        // don't pass messages to the gc unless it's our gameserver
        if (!m_serverSteamId || steamId != m_serverSteamId)
        {
            Platform::Print("NetworkingClient: ignored message from %llu (not our gs %llu)\n", steamId, m_serverSteamId);
            message->Release();
            continue;
        }

        // let the gc have a whack at it
        m_clientGC->HandleNetMessage(messageRead);

        message->Release();
    }
}

static bool ValidateTicket(std::unordered_map<uint32_t, AuthTicket> &tickets, uint64_t steamId, const void *data, uint32_t size)
{
    for (auto &pair : tickets)
    {
        if (pair.second.buffer.size() == size && !memcmp(pair.second.buffer.data(), data, size))
        {
            pair.second.steamId = steamId;
            return true;
        }
    }

    return false;
}

bool NetworkingClient::HandleMessage(uint64_t steamId, GCMessageRead &message)
{
    if (message.IsProtobuf())
    {
        // internal messages are not protobuf based
        return false;
    }

    uint32_t typeUnmasked = message.TypeUnmasked();
    if (typeUnmasked == k_EMsgNetworkConnect)
    {
        uint32_t ticketSize = message.ReadUint32();
        const void *ticket = message.ReadData(ticketSize);
        if (!message.IsValid())
        {
            Platform::Print("NetworkingClient: ignored connection from %llu (malfored message)\n", steamId);
            return true;
        }

        if (!ValidateTicket(m_tickets, steamId, ticket, ticketSize))
        {
            Platform::Print("NetworkingClient: ignored connection from %llu (ticket mismatch)\n", steamId);
            return true;
        }

        Platform::Print("NetworkingClient: sending socache to %llu\n", steamId);
        m_serverSteamId = steamId;
        m_clientGC->SendSOCacheToGameSever();

        return true;
    }

    return false;
}

void NetworkingClient::SendMessage(const GCMessageWrite &message)
{
    if (!m_serverSteamId)
    {
        // not connected to a server
        return;
    }

    // mikkotodo check return
    SteamNetworkingIdentity identity;
    identity.SetSteamID64(m_serverSteamId);

    [[maybe_unused]] EResult result = SteamNetworkingMessages()->SendMessageToUser(
        identity,
        message.Data(),
        message.Size(),
        NetMessageSendFlags,
        NetMessageChannel);

    assert(result == k_EResultOK);
}

void NetworkingClient::SetAuthTicket(uint32_t handle, const void *data, uint32_t size)
{
    AuthTicket &ticket = m_tickets[handle];

    ticket.steamId = 0;
    ticket.buffer.resize(size);
    memcpy(ticket.buffer.data(), data, size);
}

void NetworkingClient::ClearAuthTicket(uint32_t handle)
{
    auto it = m_tickets.find(handle);
    if (it == m_tickets.end())
    {
        assert(false);
        Platform::Print("NetworkingClient: tried to clear a nonexistent auth ticket???\n");
        return;
    }

    if (it->second.steamId)
    {
        Platform::Print("NetworkingClient: closing p2p session with %llu\n", it->second.steamId);

        // we had a session so close the connection
        SteamNetworkingIdentity identity;
        identity.SetSteamID64(it->second.steamId);
        SteamNetworkingMessages()->CloseChannelWithUser(identity, NetMessageChannel);

        // was this our current gameserver? if it was, clear it
        if (it->second.steamId == m_serverSteamId)
        {
            Platform::Print("NetworkingClient: clearing gs identity\n");
            m_serverSteamId = 0;
        }
    }

    m_tickets.erase(it);
}

void NetworkingClient::OnSessionRequest([[maybe_unused]] SteamNetworkingMessagesSessionRequest_t *param)
{
    if (!param->m_identityRemote.GetSteamID().BGameServerAccount())
    {
        // csgo_gc related connections come from gameservers
        return;
    }

    // accept the connection, we should receive the k_EMsgNetworkConnect message
    SteamNetworkingMessages()->AcceptSessionWithUser(param->m_identityRemote);
}

void NetworkingClient::OnSessionFailed(SteamNetworkingMessagesSessionFailed_t *param)
{
    Platform::Print("NetworkingClient::OnSessionFailed: %s\n", param->m_info.m_szEndDebug);
}
