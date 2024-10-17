#include "stdafx.h"
#include "gc_server.h"
#include "gc_const.h"
#include "gc_const_csgo.h"
#include "graffiti.h"

const char *MessageName(uint32_t type);

ServerGC::ServerGC()
    : m_networking{ this }
{
    Platform::Print("ServerGC spawned\n");

    // also called from ClientGC's constructor
    Graffiti::Initialize();
}

ServerGC::~ServerGC()
{
    Platform::Print("ServerGC destoryed\n");
}

void ServerGC::HandleMessage(uint32_t type, const void *data, uint32_t size)
{
    bool protobuf = (type & ProtobufMask);
    type &= ~ProtobufMask;

    if (protobuf)
    {
        switch (type)
        {
        case k_EMsgGCServerHello:
            OnServerHello(data, size);
            break;

        case k_EMsgGCCStrike15_v2_Server2GCClientValidate:
            // server doesn't want a response so ignore
            break;

        case k_EMsgGC_IncrementKillCountAttribute:
            IncrementKillCountAttribute(data, size);
            break;

        default:
            Platform::Print("ServerGC::HandleMessage: unhandled message %s (protobuf %s)\n",
                MessageName(type),
                protobuf ? "yes" : "no");
            break;
        }
    }
}

bool ServerGC::HasOutgoingMessages(uint32_t &size)
{
    if (m_outgoingMessages.empty())
    {
        return false;
    }

    OutgoingMessage &message = m_outgoingMessages.front();
    size = message.Size();
    return true;
}

bool ServerGC::PopOutgoingMessage(uint32_t &type, void *buffer, uint32_t bufferSize, uint32_t &size)
{
    if (m_outgoingMessages.empty())
    {
        type = 0;
        size = 0;
        return false;
    }

    OutgoingMessage &message = m_outgoingMessages.front();
    type = message.Type();
    size = message.Size();

    if (bufferSize < message.Size())
    {
        return false;
    }

    memcpy(buffer, message.Data(), message.Size());
    m_outgoingMessages.pop();
    return true;
}

void ServerGC::ClientConnected(uint64_t steamId)
{
    Platform::Print("ClientConnected: %llu\n", steamId);
    m_networking.ClientConnected(steamId);
}

void ServerGC::ClientDisconnected(uint64_t steamId)
{
    Platform::Print("ClientDisconnected: %llu\n", steamId);
    m_networking.ClientDisconnected(steamId);

    CMsgSOCacheUnsubscribed message;
    message.mutable_owner_soid()->set_type(SoIdTypeSteamId);
    message.mutable_owner_soid()->set_id(steamId);
    m_outgoingMessages.emplace(k_ESOMsg_CacheUnsubscribed, message);
}

void ServerGC::Update()
{
    m_networking.Update();
}

void ServerGC::AddOutgoingMessage(const void *data, uint32_t size)
{
    m_outgoingMessages.emplace(data, size);
}

void ServerGC::OnServerHello(const void *data, uint32_t size)
{
    CMsgServerHello hello;
    if (!ReadGCMessage(hello, data, size))
    {
        Platform::Print("Parsing CMsgServerHello failed, ignoring\n");
        return;
    }

    // we don't care about anything in this message, just reply

    CMsgCStrike15Welcome csWelcome;
    csWelcome.set_gscookieid(GameServerCookieId);

    CMsgClientWelcome welcome;
    welcome.set_version(0);
    welcome.set_game_data(csWelcome.SerializeAsString());
    welcome.set_rtime32_gc_welcome_timestamp(static_cast<uint32_t>(time(nullptr)));

    m_outgoingMessages.emplace(k_EMsgGCServerWelcome, welcome);
}

void ServerGC::IncrementKillCountAttribute(const void *data, uint32_t size)
{
    CMsgIncrementKillCountAttribute message;
    if (!ReadGCMessage(message, data, size))
    {
        Platform::Print("Parsing CMsgIncrementKillCountAttribute failed, ignoring\n");
        return;
    }

    // just forward it to the killer
    CSteamID killerId{ message.killer_account_id(), k_EUniversePublic, k_EAccountTypeIndividual };
    m_networking.SendMessage(killerId.ConvertToUint64(), k_EMsgGC_IncrementKillCountAttribute, message);
}
