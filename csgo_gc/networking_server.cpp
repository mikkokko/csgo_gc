#include "stdafx.h"
#include "networking_server.h"
#include "gc_message.h"

NetworkingServer::NetworkingServer(ISteamNetworkingMessages *networkingMessages)
    : m_networkingMessages{ networkingMessages }
    , m_sessionRequest{ this, &NetworkingServer::OnSessionRequest }
    , m_sessionFailed{ this, &NetworkingServer::OnSessionFailed }
{
}

bool NetworkingServer::ReceiveMessage(SteamNetworkingMessage_t *&message)
{
    if (!m_networkingMessages->ReceiveMessagesOnChannel(NetMessageChannel, &message, 1))
    {
        return false;
    }

    uint64_t steamId = message->m_identityPeer.GetSteamID64();

    // see if we have a session
    auto it = m_clients.find(steamId);
    if (it == m_clients.end())
    {
        Platform::Print("NetworkingServer: ignored message from %llu (no session)\n", steamId);
        message->Release();
        return false;
    }

    return true;
}

// helper for SteamNetworkingMessages::SendMessageToUser that attempts to do some kind of error handling
static void SendMessageToUser(ISteamNetworkingMessages *networkingMessages, uint64_t steamId, const GCMessageWrite &message)
{
    SteamNetworkingIdentity identity;
    identity.SetSteamID64(steamId);

    EResult result = networkingMessages->SendMessageToUser(
        identity,
        message.Data(),
        message.Size(),
        NetMessageSendFlags,
        NetMessageChannel);

    if (result != k_EResultOK)
    {
        Platform::Print("SendMessageToUser failed for %llu: %d, closing session and trying again\n", steamId, result);

        networkingMessages->CloseChannelWithUser(identity, NetMessageChannel);

        result = networkingMessages->SendMessageToUser(
            identity,
            message.Data(),
            message.Size(),
            NetMessageSendFlags,
            NetMessageChannel);

        if (result != k_EResultOK)
        {
            // not much we can do in this situation i guess
            Platform::Print("SendMessageToUser failed for %llu\n", steamId);
        }
    }
}

void NetworkingServer::ClientConnected(uint64_t steamId, const void *ticket, uint32_t ticketSize)
{
    auto [it, added] = m_clients.insert(steamId);
    if (!added)
    {
        Platform::Print("got ClientConnected for %llu but they're already on the list! ignoring\n", steamId);
        return;
    }

    // send a message, if the client has csgo_gc installed they will
    // reply with their so cache and we'll add them to our list
    GCMessageWrite messageWrite{ k_EMsgNetworkConnect };
    messageWrite.WriteUint32(ticketSize);
    messageWrite.WriteData(ticket, ticketSize);

    // FIXME: this gets sent when the client is connecting to the server, it's not uncommon for
    // the connection to time out, in which case the player's socache never gets to the server
    SendMessageToUser(m_networkingMessages, steamId, messageWrite);
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
    m_networkingMessages->CloseChannelWithUser(identity, NetMessageChannel);
}

void NetworkingServer::SendMessage(uint64_t steamId, const GCMessageWrite &message)
{
    auto it = m_clients.find(steamId);
    if (it == m_clients.end())
    {
        Platform::Print("No csgo_gc session with %llu, not sending message!!!\n");
        return;
    }

    SendMessageToUser(m_networkingMessages, steamId, message);
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

    if (!m_networkingMessages->AcceptSessionWithUser(param->m_identityRemote))
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
