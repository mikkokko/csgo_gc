#include "stdafx.h"
#include "networking_client.h"
#include "networking_shared.h"
#include "gc_client.h"

// mikkotodo rewrite this and networking_server, current code fucking sucks for a few reasons:
// - sent message types are not validated
// - message serialization is ugly
// - client blindly changes gameservers without validation
// - networking cares about game specific things like socache subs
// - server doesn't unsub client socaches every time
// - ugly interface for sending messages (should only take serialized gc messages as input)

NetworkingClient::NetworkingClient(ClientGC *clientGC)
    : m_clientGC{ clientGC }
    , m_sessionRequest{ this, &NetworkingClient::OnSessionRequest }
    , m_sessionFailed{ this, &NetworkingClient::OnSessionFailed }
{
}

void NetworkingClient::Update()
{
    SteamNetworkingMessage_t *message;
    while (SteamNetworkingMessages()->ReceiveMessagesOnChannel(MessageChannel, &message, 1))
    {
        uint64_t steamId = message->m_identityPeer.GetSteamID64();

        // check if it's from an unknown entity
        if (m_serverSteamId && steamId != m_serverSteamId)
        {
            Platform::Print("NetworkingClient: ignored message from %llu (not our gs)\n", steamId);
            message->Release();
            continue;
        }

        GCMessage gcMessage;
        NetMessageType type = ParseNetMessage(message->GetData(), message->GetSize(), gcMessage);
        if (type == NetMessageInvalid)
        {
            Platform::Print("NetworkingClient: ignored message from %llu (bad type)\n", steamId);
        }
        else if (type == NetMessageConnected)
        {
            if (!m_serverSteamId)
            {
                Platform::Print("NetworkingClient: sending socache to %llu\n", steamId);
                m_serverSteamId = steamId;
                m_clientGC->SendSOCacheToGameSever();
            }
            else
            {
                Platform::Print(
                    "NetworkingClient: ignored NetMessageConnected from %llu (already received)\n", steamId);
            }
        }
        else if (type == NetMessageDisconnected)
        {
            if (m_serverSteamId)
            {
                Platform::Print("NetworkingClient: closing session with gameserver %llu\n", steamId);

                SteamNetworkingIdentity identity;
                identity.SetSteamID64(m_serverSteamId);
                SteamNetworkingMessages()->CloseChannelWithUser(identity, MessageChannel);

                m_serverSteamId = 0;
            }
            else
            {
                Platform::Print(
                    "NetworkingClient: ignored NetMessageDisconnected from %llu (already cleared)\n", steamId);
            }
        }
        else if (type == NetMessageForGC)
        {
            m_clientGC->AddOutgoingMessage(gcMessage.data, gcMessage.size);
        }

        message->Release();
    }
}

void NetworkingClient::SendMessage(uint32_t type, const google::protobuf::MessageLite &message)
{
    if (!m_serverSteamId)
    {
        Platform::Print("NetworkingClient::SendMessage no gameserver!! ignoring\n");
        return;
    }

    type |= ProtobufMask;

    std::vector<uint8_t> buffer;
    buffer.resize(sizeof(NetMessageHeader));
    NetMessageHeader *header = reinterpret_cast<NetMessageHeader *>(buffer.data());
    header->sentinel = NetMessageSentinel;
    header->type = NetMessageForGC;

    AppendGCMessage(buffer, type, message);

    // mikkotodo check return
    SteamNetworkingIdentity identity;
    identity.SetSteamID64(m_serverSteamId);

    [[maybe_unused]] EResult result = SteamNetworkingMessages()->SendMessageToUser(
        identity,
        buffer.data(),
        buffer.size(),
        MessageSendFlags,
        MessageChannel);

    assert(result == k_EResultOK);
}

void NetworkingClient::OnSessionRequest([[maybe_unused]] SteamNetworkingMessagesSessionRequest_t *param)
{
    // don't do anything extra until we get the message
    Platform::Print("NetworkingClient::OnSessionRequest\n");
    SteamNetworkingMessages()->AcceptSessionWithUser(param->m_identityRemote);
}

void NetworkingClient::OnSessionFailed(SteamNetworkingMessagesSessionFailed_t *param)
{
    Platform::Print("NetworkingClient::OnSessionFailed: %s\n", param->m_info.m_szEndDebug);

    if (m_serverSteamId && param->m_info.m_identityRemote.GetSteamID64() == m_serverSteamId)
    {
        Platform::Print("NetworkingClient::OnSessionFailed: clear gs identity\n");
        m_serverSteamId = 0;
    }
}
