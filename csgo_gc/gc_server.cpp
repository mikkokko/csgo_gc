#include "stdafx.h"
#include "gc_server.h"
#include "gc_const.h"
#include "gc_const_csgo.h"
#include "gc_message.h"
#include "graffiti.h"
#include "networking_shared.h"

#include "base_gcmessages.pb.h"
#include "cstrike15_gcmessages.pb.h"
#include "econ_gcmessages.pb.h"
#include "gcsdk_gcmessages.pb.h"
#include "gcsystemmsgs.pb.h"

// yuck!! needed for CSteamID (construct full id from account id)
#include "steam/steamclientpublic.h"

constexpr int MaxConnectMessages = 5;
constexpr int ConnectMessageDelayBase = 5;

ServerGC::ServerGC()
{
    // also called from ClientGC's constructor
    Graffiti::Initialize();

    StartThread();

    Platform::Print("ServerGC spawned\n");
}

ServerGC::~ServerGC()
{
    StopThread();
    Platform::Print("ServerGC destroyed\n");
}

void ServerGC::HandleEvent(GCEvent type, uint64_t id, std::vector<uint8_t> &&buffer)
{
    switch (type)
    {
    case GCEvent::Message:
        HandleMessage(static_cast<uint32_t>(id), buffer.data(), static_cast<uint32_t>(buffer.size()));
        break;

    case GCEvent::NetMessage:
        HandleNetMessage(id, std::move(buffer));
        break;

    case GCEvent::ClientConnected:
        ClientConnected(id, std::move(buffer));
        break;

    case GCEvent::ClientDisconnected:
        ClientDisconnected(id);
        break;

    case GCEvent::SessionRequest:
        HandleSessionRequest(id);
        break;

    case GCEvent::Tick:
        UpdateConnections();
        break;

    default:
        assert(false);
        break;
    }
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
            SendServerWelcome();
            break;

        case k_EMsgGCCStrike15_v2_Server2GCClientValidate:
            // server doesn't want a response so ignore
            break;

        case k_EMsgGC_IncrementKillCountAttribute:
            IncrementKillCountAttribute(messageRead);
            break;

        default:
            Platform::Print("ServerGC::HandleMessage: unhandled protobuf message {})\n",
                MessageName(messageRead.TypeUnmasked()));
            break;
        }
    }
}

void ServerGC::ClientConnected(uint64_t steamId, std::vector<uint8_t> &&ticket)
{
    auto [it, inserted] = m_clients.try_emplace(steamId, std::move(ticket));
    if (!inserted)
    {
        Platform::Print("got ClientConnected for {} but they're already on the list! ignoring\n", steamId);
        return;
    }

    SendConnectMessage(steamId, it->second);
}

void ServerGC::ClientDisconnected(uint64_t steamId)
{
    if (m_clients.erase(steamId))
    {
        PostToHost(HostEvent::CloseChannel, steamId, {});
    }
    else
    {
        Platform::Print("got ClientDisconnected for {} but they're not on the list! ignoring\n", steamId);
    }

    HandleClientSOCacheUnsubscribe(steamId);
}

void ServerGC::HandleSessionRequest(uint64_t steamId)
{
    auto it = m_clients.find(steamId);
    if (it == m_clients.end())
    {
        Platform::Print("{} sent a session request, we don't have a csgo_gc session, ignoring...\n", steamId);
        return;
    }

    Platform::Print("{} sent a session request, we were playing GC with them so accept\n", steamId);

    // request latest socache just in case, this will implicitly accept the request
    SendConnectMessage(steamId, it->second);
}

void ServerGC::UpdateConnections()
{
    for (auto &[steamId, state] : m_clients)
    {
        if (state.hasSOCache || state.connectSendCount >= MaxConnectMessages)
        {
            continue;
        }

        auto nextTime = state.lastConnectSend + std::chrono::seconds{ state.connectSendCount * ConnectMessageDelayBase };
        if (std::chrono::steady_clock::now() >= nextTime)
        {
            SendConnectMessage(steamId, state);
        }
    }
}

void ServerGC::SendConnectMessage(uint64_t steamId, GCClientState &state)
{
    // send a message, if the client has csgo_gc installed they will reply with their so cache
    GCMessageWrite messageWrite{ k_EMsgNetworkConnect };
    messageWrite.WriteUint32(static_cast<uint32_t>(state.authTicket.size()));
    messageWrite.WriteData(state.authTicket.data(), static_cast<uint32_t>(state.authTicket.size()));

    state.lastConnectSend = std::chrono::steady_clock::now();
    state.connectSendCount++;
    SendMessageToClient(steamId, std::move(messageWrite).TakeBuffer());

    Platform::Print("Queued connect message {}/{} to {}\n", state.connectSendCount, MaxConnectMessages, steamId);
}

void ServerGC::SendMessageToClient(uint64_t steamId, std::vector<uint8_t> &&buffer)
{
    if (!m_clients.contains(steamId))
    {
        Platform::Print("No csgo_gc session with {}, not sending message!!!\n", steamId);
        return;
    }

    PostToHost(HostEvent::NetMessage, steamId, std::move(buffer));
}

void ServerGC::HandleClientSOCacheUnsubscribe(uint64_t steamId)
{
    Platform::Print("HandleClientSOCacheUnsubscribe: {}\n", steamId);

    CMsgSOCacheUnsubscribed message;
    message.set_owner(steamId); // for old builds
    message.mutable_owner_soid()->set_type(SoIdTypeSteamId);
    message.mutable_owner_soid()->set_id(steamId);

    GCMessageWrite write{ k_ESOMsg_CacheUnsubscribed, message };
    uint32_t typeMasked = write.TypeMasked();
    PostToHost(HostEvent::Message, typeMasked, std::move(write).TakeBuffer());
}

template<typename T>
static bool ValidateOwner(const T &message, uint64_t steamId)
{
    // these should always have owner soids (always written even when
    // running on old builds), but check it for completeness sake
    if (message.has_owner_soid())
    {
        if (message.owner_soid().type() != SoIdTypeSteamId
            || message.owner_soid().id() != steamId)
        {
            Platform::Print("ValidateMessageOwnerSOID {}: steam id mismatch (message has {})\n",
                steamId, message.owner_soid().id());
            return false;
        }
    }
    else if (message.has_owner())
    {
        if (message.owner() != steamId)
        {
            Platform::Print("ValidateMessageOwnerSOID {}: steam id mismatch (message has {})\n",
                steamId, message.owner());
            return false;
        }
    }
    else
    {
        Platform::Print("ValidateMessageOwnerSOID {}: no owner provided in message\n", steamId);
        return false;
    }

    return true;
}

template<typename T>
static bool ValidateMessageOwnerSOID(GCMessageRead &messageRead, uint64_t steamId, std::optional<GCMessageWrite> &)
{
    T message;
    if (!messageRead.ReadProtobuf(message))
    {
        Platform::Print("ValidateMessageOwnerSOID {}: parsing failed\n", steamId);
        return false;
    }

    return ValidateOwner(message, steamId);
}

// FIXME: made up
constexpr int MaxServerSOCacheItems = 64;

static bool RemoveUnequippedItems(CMsgSOCacheSubscribed &message, int &itemCount)
{
    bool modified = false;

    for (auto it = message.mutable_objects()->begin(); it != message.mutable_objects()->end(); it++)
    {
        if (it->type_id() != SOTypeItem)
        {
            continue;
        }

        for (auto obj = it->mutable_object_data()->begin(); obj != it->mutable_object_data()->end();)
        {
            CSOEconItem item;
            if (!item.ParseFromString(*obj) || !item.equipped_state_size())
            {
                obj = it->mutable_object_data()->erase(obj);
                modified = true;
            }
            else
            {
                obj++;
                itemCount++;
            }
        }
    }

    return modified;
}

template<>
bool ValidateMessageOwnerSOID<CMsgSOCacheSubscribed>(GCMessageRead &messageRead, uint64_t steamId, std::optional<GCMessageWrite> &sanitized)
{
    CMsgSOCacheSubscribed message;
    if (!messageRead.ReadProtobuf(message))
    {
        Platform::Print("ValidateMessageOwnerSOID {}: parsing failed\n", steamId);
        return false;
    }

    if (!ValidateOwner(message, steamId))
    {
        return false;
    }

    size_t oldSize = message.ByteSizeLong();

    int itemCount = 0;
    bool modified = RemoveUnequippedItems(message, itemCount);

    if (itemCount > MaxServerSOCacheItems)
    {
        Platform::Print("Client {} socache has {} items (max allowed {}), ignoring\n", steamId, itemCount, MaxServerSOCacheItems);
        return false;
    }

    if (modified)
    {
        Platform::Print("SOCache from {} had to be cleaned up ({} -> {} bytes)\n", steamId, oldSize, message.ByteSizeLong());
        sanitized.emplace(k_ESOMsg_CacheSubscribed, message);
    }

    return true;
}

void ServerGC::HandleNetMessage(uint64_t steamId, std::vector<uint8_t> &&buffer)
{
    auto it = m_clients.find(steamId);
    if (it == m_clients.end())
    {
        Platform::Print("ServerGC: ignored message from {} (no session)\n", steamId);
        return;
    }

    Platform::Print("HandleNetMessage: {}, {} bytes\n", steamId, buffer.size());

    GCMessageRead validate{ 0, buffer.data(), static_cast<uint32_t>(buffer.size()) };
    if (!validate.IsValid())
    {
        assert(false);
        return;
    }

    if (!validate.IsProtobuf())
    {
        // all the allowed messages are protobuf based
        Platform::Print("ServerGC: ignoring non protobuf message {} from {}\n",
            validate.TypeUnmasked(), steamId);
        return;
    }

    // validate the type and contents
    bool isValid = false;
    std::optional<GCMessageWrite> sanitized;

    switch (validate.TypeUnmasked())
    {
    case k_ESOMsg_Create:
    case k_ESOMsg_Update:
    case k_ESOMsg_Destroy:
        isValid = ValidateMessageOwnerSOID<CMsgSOSingleObject>(validate, steamId, sanitized);
        break;

    case k_ESOMsg_CacheSubscribed:
        isValid = ValidateMessageOwnerSOID<CMsgSOCacheSubscribed>(validate, steamId, sanitized);
        break;

    case k_ESOMsg_UpdateMultiple:
        isValid = ValidateMessageOwnerSOID<CMsgSOMultipleObjects>(validate, steamId, sanitized);
        break;

    case k_EMsgGCItemAcknowledged:
    case k_EMsgGCItemAcknowledged__DEPRECATED:
        isValid = true;
        break;
    }

    if (!isValid)
    {
        Platform::Print("ServerGC: ignoring net message {} from {}\n",
            validate.TypeUnmasked(), steamId);
        return;
    }

    if (!m_sentWelcome)
    {
        // FIXME: ideally we'd sent this on steam logon, instead of on demand...
        Platform::Print("Sending server welcome due to net message\n");
        SendServerWelcome();
    }

    if (sanitized.has_value())
    {
        // pass the sanitized message
        uint32_t typeMasked = sanitized->TypeMasked();
        PostToHost(HostEvent::Message, typeMasked, std::move(*sanitized).TakeBuffer());
    }
    else
    {
        // otherwise the old message was fine
        PostToHost(HostEvent::Message, validate.TypeMasked(), std::move(buffer));
    }

    if (validate.TypeUnmasked() == k_ESOMsg_CacheSubscribed && !it->second.hasSOCache)
    {
        Platform::Print("ServerGC: received socache from {}\n", steamId);
        it->second.hasSOCache = true;
    }
}

void ServerGC::SendServerWelcome()
{
    // we don't care about anything in this message, just reply

    CMsgCStrike15Welcome csWelcome;
    csWelcome.set_gscookieid(GameServerCookieId);

    CMsgClientWelcome welcome;
    welcome.set_version(0);
    welcome.set_game_data(csWelcome.SerializeAsString());
    welcome.set_rtime32_gc_welcome_timestamp(static_cast<uint32_t>(time(nullptr)));

    GCMessageWrite write{ k_EMsgGCServerWelcome, welcome };
    uint32_t typeMasked = write.TypeMasked();
    PostToHost(HostEvent::Message, typeMasked, std::move(write).TakeBuffer());

    m_sentWelcome = true;
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
    GCMessageWrite messageWrite{ k_EMsgGC_IncrementKillCountAttribute, message };
    CSteamID killerId{ message.killer_account_id(), k_EUniversePublic, k_EAccountTypeIndividual };
    SendMessageToClient(killerId.ConvertToUint64(), std::move(messageWrite).TakeBuffer());
}
