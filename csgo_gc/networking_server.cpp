#include "stdafx.h"
#include "networking_server.h"
#include "networking_shared.h"
#include "gc_server.h"

// mikkotodo: caches are only unsubbed on successful disconnects!!!

NetworkingServer::NetworkingServer(ServerGC *serverGC)
    : m_serverGC{ serverGC }
    , m_sessionRequest{ this, &NetworkingServer::OnSessionRequest }
    , m_sessionFailed{ this, &NetworkingServer::OnSessionFailed }
{
}

void NetworkingServer::Update()
{
    SteamNetworkingMessage_t *message;
    while (SteamGameServerNetworkingMessages()->ReceiveMessagesOnChannel(MessageChannel, &message, 1))
    {
        uint64_t steamId = message->m_identityPeer.GetSteamID64();

        // see if we have a session
        auto it = m_clients.find(steamId);
        if (it == m_clients.end())
        {
            Platform::Print("NetworkingServer: ignored message from %llu (not on list)\n", steamId);
            message->Release();
            continue;
        }

        GCMessage gcMessage;
        NetMessageType netMessageType = ParseNetMessage(message->GetData(), message->GetSize(), gcMessage);
        if (netMessageType != NetMessageForGame)
        {
            Platform::Print("NetworkingServer: ignored message from %llu (bad header)\n", steamId);
            message->Release();
            continue;
        }

        ClientInfo &info = it->second;
        if (!info.cacheSubscribed)
        {
            uint32_t type = *reinterpret_cast<const uint32_t *>(gcMessage.data);
            if (type != (k_ESOMsg_CacheSubscribed | ProtobufMask))
            {
                Platform::Print("NetworkingServer: ignored message from %llu (no cache sub %u %u)\n",
                    steamId, type, type & ~ProtobufMask);
                message->Release();
                continue;
            }

            info.cacheSubscribed = true;
        }

        // read the message (mikkotodo do checks???)
        m_serverGC->AddOutgoingMessage(gcMessage.data, gcMessage.size);
        message->Release();
    }
}

void NetworkingServer::ClientConnected(uint64_t steamId)
{
    auto result = m_clients.try_emplace(steamId);
    if (!result.second)
    {
        Platform::Print("got ClientConnected for %llu but they're already on the list! ignoring\n", steamId);
        return;
    }

    // send a message, if the client has csgo_gc installed they will
    // reply with their so cache and we'll add them to our list
    SteamNetworkingIdentity identity;
    identity.SetSteamID64(steamId);

    NetMessageHeader header;
    header.sentinel = NetMessageSentinel;
    header.type = NetMessageConnected;

    [[maybe_unused]] EResult sendResult = SteamGameServerNetworkingMessages()->SendMessageToUser(
        identity,
        &header,
        sizeof(header),
        MessageSendFlags,
        MessageChannel);

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

    // don't pull the plug, tell them and they'll pull the plug
    {
        SteamNetworkingIdentity identity;
        identity.SetSteamID64(steamId);

        NetMessageHeader header;
        header.sentinel = NetMessageSentinel;
        header.type = NetMessageDisconnected;

        [[maybe_unused]] EResult sendResult = SteamGameServerNetworkingMessages()->SendMessageToUser(identity,
            &header,
            sizeof(header),
            MessageSendFlags,
            MessageChannel);

        assert(sendResult == k_EResultOK);
    }
}

void NetworkingServer::SendMessage(uint64_t steamId, uint32_t type, const google::protobuf::MessageLite &message)
{
    auto it = m_clients.find(steamId);
    if (it == m_clients.end())
    {
        Platform::Print("No csgo_gc session with %llu, not sending message!!!\n");
        return;
    }

    type |= ProtobufMask;

    std::vector<uint8_t> buffer;
    buffer.resize(sizeof(NetMessageHeader));
    NetMessageHeader *header = reinterpret_cast<NetMessageHeader *>(buffer.data());
    header->sentinel = NetMessageSentinel;
    header->type = NetMessageForGC; // ClientGC

    AppendGCMessage(buffer, type, message);

    // mikkotodo check return
    SteamNetworkingIdentity identity;
    identity.SetSteamID64(steamId);

    [[maybe_unused]] EResult result = SteamGameServerNetworkingMessages()->SendMessageToUser(
        identity,
        buffer.data(),
        buffer.size(),
        MessageSendFlags,
        MessageChannel);

    assert(result == k_EResultOK);
}

void NetworkingServer::OnSessionRequest(SteamNetworkingMessagesSessionRequest_t *param)
{
    uint64_t steamId = param->m_identityRemote.GetSteamID64();

    auto it = m_clients.find(steamId);
    if (it == m_clients.end())
    {
        Platform::Print("%llu sent a session request, we didn't have a csgo_gc session, ignoring\n");
        return;
    }

    Platform::Print("%llu sent a session request, we were playing GC with them so accept\n");

    if (!SteamGameServerNetworkingMessages()->AcceptSessionWithUser(param->m_identityRemote))
    {
        Platform::Print(
            "AcceptSessionWithUser failed??? removing %llu from list\n", param->m_identityRemote.GetSteamID64());
        m_clients.erase(it);
    }
}

void NetworkingServer::OnSessionFailed(SteamNetworkingMessagesSessionFailed_t *param)
{
    Platform::Print("OnSessionFailed: %s\n", param->m_info.m_szEndDebug);

    uint64_t steamId = param->m_info.m_identityRemote.GetSteamID64();

    auto it = m_clients.find(steamId);
    if (it == m_clients.end())
    {
        Platform::Print("%llu session failed, we didn't have a csgo_gc session, ignoring\n");
        return;
    }

    Platform::Print("%llu session failed, we had a csgo_gc session so cleanup\n");
    m_clients.erase(it);
}
