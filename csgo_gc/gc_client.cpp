#include "stdafx.h"
#include "gc_client.h"
#include "graffiti.h"
#include "keyvalue.h"

const char *MessageName(uint32_t type);

ClientGC::ClientGC(uint64_t steamId, ISteamNetworkingMessages *networkingMessages)
    : m_steamId{ steamId }
    , m_networking{ this, networkingMessages }
    , m_inventory{ steamId, m_config }
{
    Platform::Print("ClientGC spawned for user %llu\n", steamId);

    // also called from ServerGC's constructor
    Graffiti::Initialize();
}

ClientGC::~ClientGC()
{
    Platform::Print("ClientGC destroyed\n");
}

void ClientGC::HandleMessage(uint32_t type, const void *data, uint32_t size)
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
        case k_EMsgGCClientHello:
            OnClientHello(messageRead);
            break;

        case k_EMsgGCAdjustItemEquippedState:
            AdjustItemEquippedState(messageRead);
            break;

        case k_EMsgGCCStrike15_v2_ClientPlayerDecalSign:
            ClientPlayerDecalSign(messageRead);
            break;

        case k_EMsgGCUseItemRequest:
            UseItemRequest(messageRead);
            break;

        case k_EMsgGCCStrike15_v2_ClientRequestJoinServerData:
            ClientRequestJoinServerData(messageRead);
            break;

        case k_EMsgGCSetItemPositions:
            SetItemPositions(messageRead);
            break;

        case k_EMsgGCApplySticker:
            ApplySticker(messageRead);
            break;

        case k_EMsgGCStoreGetUserData:
            StoreGetUserData(messageRead);
            break;

        default:
            Platform::Print("ClientGC::HandleMessage: unhandled protobuf message %s\n",
                MessageName(messageRead.TypeUnmasked()));
            break;
        }
    }
    else
    {
        switch (messageRead.TypeUnmasked())
        {
        case k_EMsgGCUnlockCrate:
            UnlockCrate(messageRead);
            break;

        case k_EMsgGCNameItem:
            NameItem(messageRead);
            break;

        case k_EMsgGCNameBaseItem:
            NameBaseItem(messageRead);
            break;

        case k_EMsgGCRemoveItemName:
            RemoveItemName(messageRead);
            break;

        default:
            Platform::Print("ClientGC::HandleMessage: unhandled struct message %s\n",
                MessageName(messageRead.TypeUnmasked()));
            break;
        }
    }
}

void ClientGC::Update()
{
    m_networking.Update();
}

void ClientGC::SendSOCacheToGameSever()
{
    CMsgSOCacheSubscribed message;
    m_inventory.BuildCacheSubscription(message, m_config.Level(), true);

    GCMessageWrite messageWrite{ k_ESOMsg_CacheSubscribed, message };
    m_networking.SendMessage(messageWrite);
}

void ClientGC::HandleNetMessage(GCMessageRead &messageRead)
{
    assert(messageRead.IsValid());

    if (messageRead.IsProtobuf())
    {
        switch (messageRead.TypeUnmasked())
        {
        case k_EMsgGC_IncrementKillCountAttribute:
            IncrementKillCountAttribute(messageRead);
            return;
        }
    }

    Platform::Print("ClientGC::HandleNetMessage: unhandled protobuf message %s\n",
        MessageName(messageRead.TypeUnmasked()));
}

void ClientGC::SetAuthTicket(uint32_t handle, const void *data, uint32_t size)
{
    m_networking.SetAuthTicket(handle, data, size);
}

void ClientGC::ClearAuthTicket(uint32_t handle)
{
    m_networking.ClearAuthTicket(handle);
}

void ClientGC::SendMessageToGame(bool sendToGameServer, uint32_t type, const google::protobuf::MessageLite &message)
{
    const GCMessageWrite &messageWrite = m_outgoingMessages.emplace(type, message);

    if (sendToGameServer)
    {
        m_networking.SendMessage(messageWrite);
    }
}

constexpr uint32_t MakeAddress(uint32_t v1, uint32_t v2, uint32_t v3, uint32_t v4)
{
    return v4 | (v3 << 8) | (v2 << 16) | (v1 << 24);
}

static void BuildCSWelcome(CMsgCStrike15Welcome &message)
{
    // mikkotodo cleanup dox
    message.set_store_item_hash(136617352);
    message.set_timeplayedconsecutively(0);
    message.set_time_first_played(1329845773);
    message.set_last_time_played(1680260376);
    message.set_last_ip_address(MakeAddress(127, 0, 0, 1));
}

void ClientGC::BuildMatchmakingHello(CMsgGCCStrike15_v2_MatchmakingGC2ClientHello &message)
{
    message.set_account_id(AccountId());

    // this is the state of csgo matchmaking in 2024
    message.mutable_global_stats()->set_players_online(0);
    message.mutable_global_stats()->set_servers_online(0);
    message.mutable_global_stats()->set_players_searching(0);
    message.mutable_global_stats()->set_servers_available(0);
    message.mutable_global_stats()->set_ongoing_matches(0);
    message.mutable_global_stats()->set_search_time_avg(0);

    // don't write search_statistics

    message.mutable_global_stats()->set_main_post_url("");

    // bullshit
    message.mutable_global_stats()->set_required_appid_version(13857);
    message.mutable_global_stats()->set_pricesheet_version(1680057676); // mikkotodo revisit
    message.mutable_global_stats()->set_twitch_streams_version(2);
    message.mutable_global_stats()->set_active_tournament_eventid(20);
    message.mutable_global_stats()->set_active_survey_id(0);
    message.mutable_global_stats()->set_required_appid_version2(13862); // csgo s2

    message.set_vac_banned(m_config.VacBanned());
    message.mutable_commendation()->set_cmd_friendly(m_config.CommendedFriendly());
    message.mutable_commendation()->set_cmd_teaching(m_config.CommendedTeaching());
    message.mutable_commendation()->set_cmd_leader(m_config.CommendedLeader());
    message.set_player_level(m_config.Level());
    message.set_player_cur_xp(m_config.Xp());
}

void ClientGC::BuildClientWelcome(CMsgClientWelcome &message, const CMsgCStrike15Welcome &csWelcome,
    const CMsgGCCStrike15_v2_MatchmakingGC2ClientHello &matchmakingHello)
{
    // mikkotodo remove dox
    message.set_version(0); // this is accurate
    message.set_game_data(csWelcome.SerializeAsString());
    m_inventory.BuildCacheSubscription(*message.add_outofdate_subscribed_caches(), m_config.Level(), false);
    message.mutable_location()->set_latitude(65.0133006f);
    message.mutable_location()->set_longitude(25.4646212f);
    message.mutable_location()->set_country("FI"); // finland
    message.set_game_data2(matchmakingHello.SerializeAsString());
    message.set_rtime32_gc_welcome_timestamp(static_cast<uint32_t>(time(nullptr)));
    message.set_currency(2); // euros
    message.set_txn_country_code("FI"); // finland
}

void ClientGC::SendRankUpdate()
{
    CMsgGCCStrike15_v2_ClientGCRankUpdate message;

    PlayerRankingInfo *rank = message.add_rankings();
    rank->set_account_id(AccountId());
    rank->set_rank_id(m_config.CompetitiveRank());
    rank->set_wins(m_config.CompetitiveWins());
    rank->set_rank_type_id(RankTypeCompetitive);

    rank = message.add_rankings();
    rank->set_account_id(AccountId());
    rank->set_rank_id(m_config.WingmanRank());
    rank->set_wins(m_config.WingmanWins());
    rank->set_rank_type_id(RankTypeWingman);

    rank = message.add_rankings();
    rank->set_account_id(AccountId());
    rank->set_rank_id(m_config.DangerZoneRank());
    rank->set_wins(m_config.DangerZoneWins());
    rank->set_rank_type_id(RankTypeDangerZone);

    SendMessageToGame(false, k_EMsgGCCStrike15_v2_ClientGCRankUpdate, message);
}

void ClientGC::OnClientHello(GCMessageRead &messageRead)
{
    CMsgClientHello hello;
    if (!messageRead.ReadProtobuf(hello))
    {
        Platform::Print("Parsing CMsgClientHello failed, ignoring\n");
        return;
    }

    // we don't care about anything in this message, just reply
    CMsgCStrike15Welcome csWelcome;
    BuildCSWelcome(csWelcome);

    CMsgGCCStrike15_v2_MatchmakingGC2ClientHello mmHello;
    BuildMatchmakingHello(mmHello);

    CMsgClientWelcome clientWelcome;
    BuildClientWelcome(clientWelcome, csWelcome, mmHello);

    SendMessageToGame(false, k_EMsgGCClientWelcome, clientWelcome);

    // the real gc sends this a bit later when it has more info to put on it
    // however we have everything at our fingertips so send it right away
    // mikkotodo is this even needed? k_EMsgGCClientWelcome should have it all already
    SendMessageToGame(false, k_EMsgGCCStrike15_v2_MatchmakingGC2ClientHello, mmHello);

    // send all ranks here as well, it's a bit back and forth with real gc
    SendRankUpdate();
}

void ClientGC::AdjustItemEquippedState(GCMessageRead &messageRead)
{
    CMsgAdjustItemEquippedState message;
    if (!messageRead.ReadProtobuf(message))
    {
        Platform::Print("Parsing CMsgAdjustItemEquippedState failed, ignoring\n");
        return;
    }

    CMsgSOMultipleObjects update;
    if (!m_inventory.EquipItem(message.item_id(), message.new_class(), message.new_slot(), update))
    {
        // no change
        assert(false);
        return;
    }

    // let the gameserver know, too
    SendMessageToGame(true, k_ESOMsg_UpdateMultiple, update);
}

void ClientGC::ClientPlayerDecalSign(GCMessageRead &messageRead)
{
    CMsgGCCStrike15_v2_ClientPlayerDecalSign message;
    if (!messageRead.ReadProtobuf(message))
    {
        Platform::Print("Parsing CMsgGCCStrike15_v2_ClientPlayerDecalSign failed, ignoring\n");
        return;
    }

    if (!Graffiti::SignMessage(*message.mutable_data()))
    {
        Platform::Print("Could not sign graffiti! it won't appear\n");
        return;
    }

    SendMessageToGame(false, k_EMsgGCCStrike15_v2_ClientPlayerDecalSign, message);
}

void ClientGC::UseItemRequest(GCMessageRead &messageRead)
{
    CMsgUseItem message;
    if (!messageRead.ReadProtobuf(message))
    {
        Platform::Print("Parsing CMsgUseItem failed, ignoring\n");
        return;
    }

    CMsgSOSingleObject destroy;
    CMsgSOMultipleObjects updateMultiple;
    CMsgGCItemCustomizationNotification notification;

    if (m_inventory.UseItem(message.item_id(), destroy, updateMultiple, notification))
    {
        SendMessageToGame(true, k_ESOMsg_Destroy, destroy);
        SendMessageToGame(true, k_ESOMsg_UpdateMultiple, updateMultiple);

        SendMessageToGame(false, k_EMsgGCItemCustomizationNotification, notification);
    }
}

static void AddressString(uint32_t ip, uint32_t port, char *buffer, size_t bufferSize)
{
    snprintf(buffer, bufferSize,
        "%u.%u.%u.%u:%u\n",
        (ip >> 24) & 0xff,
        (ip >> 16) & 0xff,
        (ip >> 8) & 0xff,
        ip & 0xff,
        port);
}

void ClientGC::ClientRequestJoinServerData(GCMessageRead &messageRead)
{
    CMsgGCCStrike15_v2_ClientRequestJoinServerData request;
    if (!messageRead.ReadProtobuf(request))
    {
        Platform::Print("Parsing CMsgGCCStrike15_v2_ClientRequestJoinServerData failed, ignoring\n");
        return;
    }

    CMsgGCCStrike15_v2_ClientRequestJoinServerData response = request;
    response.mutable_res()->set_serverid(request.version());
    response.mutable_res()->set_direct_udp_ip(request.server_ip());
    response.mutable_res()->set_direct_udp_port(request.server_port());
    response.mutable_res()->set_reservationid(GameServerCookieId);

    char addressString[32];
    AddressString(request.server_ip(), request.server_port(), addressString, sizeof(addressString));
    response.mutable_res()->set_server_address(addressString);

    SendMessageToGame(false, k_EMsgGCCStrike15_v2_ClientRequestJoinServerData, response);
}

void ClientGC::SetItemPositions(GCMessageRead &messageRead)
{
    CMsgSetItemPositions message;
    if (!messageRead.ReadProtobuf(message))
    {
        Platform::Print("Parsing CMsgSetItemPositions failed, ignoring\n");
        return;
    }

    std::vector<CMsgItemAcknowledged> acknowledgements;
    acknowledgements.reserve(message.item_positions_size());

    CMsgSOMultipleObjects update;
    if (m_inventory.SetItemPositions(message, acknowledgements, update))
    {
        for (const CMsgItemAcknowledged &acknowledgement : acknowledgements)
        {
            // send these to the server only
            GCMessageWrite messageWrite{ k_EMsgGCItemAcknowledged, acknowledgement };
            m_networking.SendMessage(messageWrite);
        }

        SendMessageToGame(true, k_ESOMsg_UpdateMultiple, update);
    }
    else
    {
        assert(false);
    }
}

void ClientGC::IncrementKillCountAttribute(GCMessageRead &messageRead)
{
    CMsgIncrementKillCountAttribute message;
    if (!messageRead.ReadProtobuf(message))
    {
        Platform::Print("Parsing CMsgIncrementKillCountAttribute failed, ignoring\n");
        return;
    }

    assert(message.event_type() == 0);

    CMsgSOSingleObject update;
    if (m_inventory.IncrementKillCountAttribute(message.item_id(), message.amount(), update))
    {
        SendMessageToGame(true, k_ESOMsg_Update, update);
    }
    else
    {
        assert(false);
    }
}

void ClientGC::ApplySticker(GCMessageRead &messageRead)
{
    CMsgApplySticker message;
    if (!messageRead.ReadProtobuf(message))
    {
        Platform::Print("Parsing CMsgApplySticker failed, ignoring\n");
        return;
    }

    assert(!message.item_item_id() != !message.baseitem_defidx());

    CMsgSOSingleObject update, destroy;
    CMsgGCItemCustomizationNotification notification;

    if (!message.sticker_item_id())
    {
        // scrape
        if (m_inventory.ScrapeSticker(message, update, destroy, notification))
        {
            if (destroy.has_type_id())
            {
                // destroying a default item
                SendMessageToGame(true, k_ESOMsg_Destroy, destroy);
            }

            if (update.has_type_id())
            {
                // if the item got removed (handled above), nothing gets updated
                SendMessageToGame(true, k_ESOMsg_Update, update);
            }

            if (notification.has_request())
            {
                // might get a k_EGCItemCustomizationNotification_RemoveSticker
                SendMessageToGame(false, k_EMsgGCItemCustomizationNotification, notification);
            }
        }
        else
        {
            assert(false);
        }
    }
    else if (m_inventory.ApplySticker(message, update, destroy, notification))
    {
        SendMessageToGame(true, k_ESOMsg_Destroy, destroy);
        SendMessageToGame(true, k_ESOMsg_Update, update);

        SendMessageToGame(false, k_EMsgGCItemCustomizationNotification, notification);
    }
    else
    {
        assert(false);
    }
}

void ClientGC::StoreGetUserData(GCMessageRead &messageRead)
{
    CMsgStoreGetUserData message;
    if (!messageRead.ReadProtobuf(message))
    {
        Platform::Print("Parsing CMsgStoreGetUserData failed, ignoring\n");
        return;
    }

    KeyValue priceSheet{ "price_sheet" };
    if (!priceSheet.ParseFromFile("csgo_gc/price_sheet.txt"))
    {
        return;
    }

    std::string binaryString;
    binaryString.reserve(1 << 17);
    priceSheet.BinaryWriteToString(binaryString);

    // fuck you idiot
    CMsgStoreGetUserDataResponse response;
    response.set_result(1);
    response.set_price_sheet_version(1729); // what
    *response.mutable_price_sheet() = std::move(binaryString);

    SendMessageToGame(false, k_EMsgGCStoreGetUserDataResponse, response);
}

void ClientGC::UnlockCrate(GCMessageRead &messageRead)
{
    uint64_t keyId = messageRead.ReadUint64();
    uint64_t crateId = messageRead.ReadUint64();
    if (!messageRead.IsValid())
    {
        Platform::Print("Parsing CMsgGCUnlockCrate failed, ignoring\n");
        return;
    }

    Platform::Print("CASE OPENING %llu with %llu\n", crateId, keyId);

    CMsgSOSingleObject destroyCrate, destroyKey, newItem;
    CMsgGCItemCustomizationNotification notification;

    if (m_inventory.UnlockCrate(
            crateId,
            keyId,
            destroyCrate,
            destroyKey,
            newItem,
            notification))
    {
        // mikkotodo what does the server want to know
        SendMessageToGame(true, k_ESOMsg_Destroy, destroyCrate);
        SendMessageToGame(true, k_ESOMsg_Destroy, destroyKey);
        SendMessageToGame(true, k_ESOMsg_Create, newItem);

        SendMessageToGame(false, k_EMsgGCItemCustomizationNotification, notification);
    }
    else
    {
        assert(false);
    }
}

void ClientGC::NameItem(GCMessageRead &messageRead)
{
    uint64_t nameTagId = messageRead.ReadUint64();
    uint64_t itemId = messageRead.ReadUint64();
    messageRead.ReadData(1); // skip the sentinel
    std::string_view name = messageRead.ReadString();

    if (!messageRead.IsValid())
    {
        Platform::Print("Parsing CMsgGCNameItem failed, ignoring\n");
        return;
    }

    CMsgSOSingleObject update, destroy;
    CMsgGCItemCustomizationNotification notification;
    if (m_inventory.NameItem(nameTagId, itemId, name, update, destroy, notification))
    {
        SendMessageToGame(true, k_ESOMsg_Update, update);
        SendMessageToGame(true, k_ESOMsg_Destroy, destroy);

        SendMessageToGame(false, k_EMsgGCItemCustomizationNotification, notification);
    }
    else
    {
        assert(false);
    }
}

void ClientGC::NameBaseItem(GCMessageRead &messageRead)
{
    uint64_t nameTagId = messageRead.ReadUint64();
    uint32_t defIndex = messageRead.ReadUint32();
    messageRead.ReadData(1); // skip the sentinel
    std::string_view name = messageRead.ReadString();

    if (!messageRead.IsValid())
    {
        Platform::Print("Parsing CMsgGCNameBaseItem failed, ignoring\n");
        return;
    }

    CMsgSOSingleObject create, destroy;
    CMsgGCItemCustomizationNotification notification;
    if (m_inventory.NameBaseItem(nameTagId, defIndex, name, create, destroy, notification))
    {
        SendMessageToGame(true, k_ESOMsg_Create, create);
        SendMessageToGame(true, k_ESOMsg_Destroy, destroy);

        SendMessageToGame(false, k_EMsgGCItemCustomizationNotification, notification);
    }
    else
    {
        assert(false);
    }
}

void ClientGC::RemoveItemName(GCMessageRead &messageRead)
{
    uint64_t itemId = messageRead.ReadUint64();
    if (!messageRead.IsValid())
    {
        Platform::Print("Parsing CMsgGCRemoveItemName failed, ignoring\n");
        return;
    }

    CMsgSOSingleObject update, destroy;
    CMsgGCItemCustomizationNotification notification;
    if (m_inventory.RemoveItemName(itemId, update, destroy, notification))
    {
        if (update.has_type_id())
        {
            SendMessageToGame(true, k_ESOMsg_Update, update);
        }

        if (destroy.has_type_id())
        {
            SendMessageToGame(true, k_ESOMsg_Destroy, destroy);
        }

        SendMessageToGame(false, k_EMsgGCItemCustomizationNotification, notification);
    }
    else
    {
        assert(false);
    }
}
