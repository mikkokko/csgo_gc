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
    GCMessageRead messageRead{ type, data, size };
    if (!messageRead.IsValid())
    {
        assert(false);
        return;
    }

    if (messageRead.IsProtobuf())
    {
        switch (messageRead.TypeUnmasked())
        {
        case k_EMsgGCServerHello:
            OnServerHello(messageRead);
            break;

        case k_EMsgGCCStrike15_v2_Server2GCClientValidate:
            // server doesn't want a response so ignore
            break;

        case k_EMsgGC_IncrementKillCountAttribute:
            IncrementKillCountAttribute(messageRead);
            break;

        default:
            Platform::Print("ServerGC::HandleMessage: unhandled protobuf message %s)\n",
                MessageName(messageRead.TypeUnmasked()));
            break;
        }
    }
}

void ServerGC::ClientConnected(uint64_t steamId, const void *ticket, uint32_t ticketSize)
{
    Platform::Print("ClientConnected: %llu\n", steamId);
    m_networking.ClientConnected(steamId, ticket, ticketSize);
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
    if (!m_receivedHello)
    {
        // we're not up yet
        return;
    }

    m_networking.Update();
}

void ServerGC::HandleNetMessage(const void *data, uint32_t size)
{
    GCMessageRead validate{ 0, data, size };
    if (!validate.IsValid())
    {
        assert(false);
        return;
    }

    if (!validate.IsProtobuf())
    {
        // all the allowed messages are protobuf based
        return;
    }

    // mikkotodo validate the message contents...
    // currently you can modify the inventories of other clients
    switch (validate.TypeUnmasked())
    {
    case k_ESOMsg_Create:
    case k_ESOMsg_Update:
    case k_ESOMsg_Destroy:
    case k_ESOMsg_CacheSubscribed:
    case k_ESOMsg_UpdateMultiple:
        break;

    case k_EMsgGCItemAcknowledged:
        // for the case opening message
        break;

    default:
        Platform::Print("ServerGC::HandleNetMessage: ignoring message %u\n",
            validate.TypeUnmasked());
        return;
    }

    m_outgoingMessages.emplace(data, size);
}

void ServerGC::OnServerHello(GCMessageRead &messageRead)
{
    CMsgServerHello hello;
    if (!messageRead.ReadProtobuf(hello))
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

    m_receivedHello = true;
}

void ServerGC::IncrementKillCountAttribute(GCMessageRead &messageRead)
{
    CMsgIncrementKillCountAttribute message;
    if (!messageRead.ReadProtobuf(message))
    {
        Platform::Print("Parsing CMsgIncrementKillCountAttribute failed, ignoring\n");
        return;
    }

    // just forward it to the killer
    GCMessageWrite messageWrite{ k_EMsgGC_IncrementKillCountAttribute , message };
    CSteamID killerId{ message.killer_account_id(), k_EUniversePublic, k_EAccountTypeIndividual };
    m_networking.SendMessage(killerId.ConvertToUint64(), messageWrite);
}
