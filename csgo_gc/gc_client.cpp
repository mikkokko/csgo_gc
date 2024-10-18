#include "stdafx.h"
#include "gc_client.h"
#include "graffiti.h"

const char *MessageName(uint32_t type);

ClientGC::ClientGC(uint64_t steamId)
    : m_steamId{ steamId }
    , m_inventory{ steamId }
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
    bool protobuf = (type & ProtobufMask);
    type &= ~ProtobufMask;

    if (protobuf)
    {
        switch (type)
        {
        case k_EMsgGCClientHello:
            OnClientHello(data, size);
            break;

        case k_EMsgGCAdjustItemEquippedState:
            AdjustItemEquippedState(data, size);
            break;

        case k_EMsgGCCStrike15_v2_ClientPlayerDecalSign:
            ClientPlayerDecalSign(data, size);
            break;

        case k_EMsgGCUseItemRequest:
            UseItemRequest(data, size);
            break;

        case k_EMsgGCCStrike15_v2_ClientRequestJoinServerData:
            ClientRequestJoinServerData(data, size);
            break;

        case k_EMsgGCSetItemPositions:
            SetItemPositions(data, size);
            break;

        case k_EMsgGC_IncrementKillCountAttribute:
            IncrementKillCountAttribute(data, size);
            break;

        case k_EMsgGCApplySticker:
            ApplySticker(data, size);
            break;

        default:
            Platform::Print("ClientGC::HandleMessage: unhandled protobuf message %s\n", MessageName(type));
            break;
        }
    }
    else
    {
        switch (type)
        {

        case k_EMsgGCUnlockCrate:
            UnlockCrate(data, size);
            break;

        default:
            Platform::Print("ClientGC::HandleMessage: unhandled struct message %s\n", MessageName(type));
            break;
        }
    }
}

bool ClientGC::HasOutgoingMessages(uint32_t &size)
{
    if (m_outgoingMessages.empty())
    {
        return false;
    }

    OutgoingMessage &message = m_outgoingMessages.front();
    size = message.Size();
    return true;
}

bool ClientGC::PopOutgoingMessage(uint32_t &type, void *buffer, uint32_t bufferSize, uint32_t &size)
{
    if (m_outgoingMessages.empty())
    {
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

void ClientGC::Update()
{
    m_networking.Update();
}

void ClientGC::SendSOCacheToGameSever()
{
    CMsgSOCacheSubscribed message;
    m_inventory.BuildCacheSubscription(message, m_config.Level(), true);
    m_networking.SendMessage(k_ESOMsg_CacheSubscribed, message);
}

void ClientGC::AddOutgoingMessage(const void *data, uint32_t size)
{
    m_outgoingMessages.emplace(data, size);
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
    message.mutable_global_stats()->set_pricesheet_version(1680057676);
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

    m_outgoingMessages.emplace(k_EMsgGCCStrike15_v2_ClientGCRankUpdate, message);
}

void ClientGC::OnClientHello(const void *data, uint32_t size)
{
    CMsgClientHello hello;
    if (!ReadGCMessage(hello, data, size))
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

    m_outgoingMessages.emplace(k_EMsgGCClientWelcome, clientWelcome);

    // the real gc sends this a bit later when it has more info to put on it
    // however we have everything at our fingertips so send it right away
    // mikkotodo is this even needed? k_EMsgGCClientWelcome should have it all already
    m_outgoingMessages.emplace(k_EMsgGCCStrike15_v2_MatchmakingGC2ClientHello, mmHello);

    // send all ranks here as well, it's a bit back and forth with real gc
    SendRankUpdate();
}

void ClientGC::AdjustItemEquippedState(const void *data, uint32_t size)
{
    CMsgAdjustItemEquippedState message;
    if (!ReadGCMessage(message, data, size))
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

    m_outgoingMessages.emplace(k_ESOMsg_UpdateMultiple, update);

    // let the gameserver know, too
    m_networking.SendMessage(k_ESOMsg_UpdateMultiple, update);
}

void ClientGC::ClientPlayerDecalSign(const void *data, uint32_t size)
{
    CMsgGCCStrike15_v2_ClientPlayerDecalSign message;
    if (!ReadGCMessage(message, data, size))
    {
        Platform::Print("Parsing CMsgGCCStrike15_v2_ClientPlayerDecalSign failed, ignoring\n");
        return;
    }

    if (!Graffiti::SignMessage(*message.mutable_data()))
    {
        Platform::Print("Could not sign graffiti! it won't appear\n");
        return;
    }

    m_outgoingMessages.emplace(k_EMsgGCCStrike15_v2_ClientPlayerDecalSign, message);
}

void ClientGC::UseItemRequest(const void *data, uint32_t size)
{
    CMsgUseItem message;
    if (!ReadGCMessage(message, data, size))
    {
        Platform::Print("Parsing CMsgUseItem failed, ignoring\n");
        return;
    }

    CMsgSOSingleObject destroy;
    CMsgSOMultipleObjects updateMultiple;
    CMsgGCItemCustomizationNotification notification;

    if (m_inventory.UseItem(message.item_id(), destroy, updateMultiple, notification))
    {
        m_outgoingMessages.emplace(k_ESOMsg_Destroy, destroy);
        m_networking.SendMessage(k_ESOMsg_Destroy, destroy);

        m_outgoingMessages.emplace(k_ESOMsg_UpdateMultiple, updateMultiple);
        m_networking.SendMessage(k_ESOMsg_UpdateMultiple, updateMultiple);

        m_outgoingMessages.emplace(k_EMsgGCItemCustomizationNotification, notification);
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

void ClientGC::ClientRequestJoinServerData(const void *data, uint32_t size)
{
    CMsgGCCStrike15_v2_ClientRequestJoinServerData request;
    if (!ReadGCMessage(request, data, size))
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

    m_outgoingMessages.emplace(k_EMsgGCCStrike15_v2_ClientRequestJoinServerData, response);
}

void ClientGC::SetItemPositions(const void *data, uint32_t size)
{
    CMsgSetItemPositions message;
    if (!ReadGCMessage(message, data, size))
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
            m_networking.SendMessage(k_EMsgGCItemAcknowledged, acknowledgement);
        }

        m_outgoingMessages.emplace(k_ESOMsg_UpdateMultiple, update);
        m_networking.SendMessage(k_ESOMsg_UpdateMultiple, update);
    }
    else
    {
        assert(false);
    }
}

void ClientGC::IncrementKillCountAttribute(const void *data, uint32_t size)
{
    CMsgIncrementKillCountAttribute message;
    if (!ReadGCMessage(message, data, size))
    {
        Platform::Print("Parsing CMsgIncrementKillCountAttribute failed, ignoring\n");
        return;
    }

    assert(message.event_type() == 0);

    CMsgSOSingleObject update;
    if (m_inventory.IncrementKillCountAttribute(message.item_id(), message.amount(), update))
    {
        m_outgoingMessages.emplace(k_ESOMsg_Update, update);
        m_networking.SendMessage(k_ESOMsg_Update, update);
    }
    else
    {
        assert(false);
    }
}


void ClientGC::ApplySticker(const void *data, uint32_t size)
{
    CMsgApplySticker message;
    if (!ReadGCMessage(message, data, size))
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
                m_outgoingMessages.emplace(k_ESOMsg_Destroy, destroy);
                m_networking.SendMessage(k_ESOMsg_Destroy, destroy);
            }

            if (update.has_type_id())
            {
                // if the item got removed (handled above), nothing gets updated
                m_outgoingMessages.emplace(k_ESOMsg_Update, update);
                m_networking.SendMessage(k_ESOMsg_Update, update);
            }

            if (notification.has_request())
            {
                // might get a k_EGCItemCustomizationNotification_RemoveSticker
                m_outgoingMessages.emplace(k_EMsgGCItemCustomizationNotification, notification);
            }
        }
        else
        {
            assert(false);
        }
    }
    else if (m_inventory.ApplySticker(message, update, destroy, notification))
    {
        m_outgoingMessages.emplace(k_ESOMsg_Destroy, destroy);
        m_networking.SendMessage(k_ESOMsg_Destroy, destroy);

        m_outgoingMessages.emplace(k_ESOMsg_Update, update);
        m_networking.SendMessage(k_ESOMsg_Update, update);

        m_outgoingMessages.emplace(k_EMsgGCItemCustomizationNotification, notification);
    }
    else
    {
        assert(false);
    }
}

void ClientGC::UnlockCrate(const void *data, uint32_t size)
{
    const CMsgGCUnlockCrate *message = ReadGameStructMessage<CMsgGCUnlockCrate>(data, size);
    if (!message)
    {
        Platform::Print("Parsing CMsgGCUnlockCrate failed, ignoring\n");
        return;
    }

    Platform::Print("CASE OPENING %llu with %llu\n", message->crate_id, message->key_id);

    CMsgSOSingleObject destroyCrate, destroyKey, newItem;
    CMsgGCItemCustomizationNotification notification;

    if (m_inventory.UnlockCrate(
        message->crate_id,
        message->key_id,
        destroyCrate,
        destroyKey,
        newItem,
        notification))
    {
        // mikkotodo what does the server want to know
        m_outgoingMessages.emplace(k_ESOMsg_Destroy, destroyCrate);
        m_networking.SendMessage(k_ESOMsg_Destroy, destroyCrate);
        
        m_outgoingMessages.emplace(k_ESOMsg_Destroy, destroyKey);
        m_networking.SendMessage(k_ESOMsg_Destroy, destroyKey);

        m_outgoingMessages.emplace(k_ESOMsg_Create, newItem);
        m_networking.SendMessage(k_ESOMsg_Create, newItem);

        CMsgRequestInventoryRefresh empty; // just an empty message (mikkotodo fix)
        m_outgoingMessages.emplace(k_EMsgGCUnlockCrateResponse, empty);

        m_outgoingMessages.emplace(k_EMsgGCItemCustomizationNotification, notification);
    }
    else
    {
        assert(false);
    }
}

const char *MessageName(uint32_t type)
{
    switch (type)
    {
#define HANDLE_MSG(e) \
    case e: \
        return #e
        HANDLE_MSG(k_EMsgGCSystemMessage); // 4001;
        HANDLE_MSG(k_EMsgGCReplicateConVars); // 4002;
        HANDLE_MSG(k_EMsgGCConVarUpdated); // 4003;
        HANDLE_MSG(k_EMsgGCInQueue); // 4008;
        HANDLE_MSG(k_EMsgGCInviteToParty); // 4501;
        HANDLE_MSG(k_EMsgGCInvitationCreated); // 4502;
        HANDLE_MSG(k_EMsgGCPartyInviteResponse); // 4503;
        HANDLE_MSG(k_EMsgGCKickFromParty); // 4504;
        HANDLE_MSG(k_EMsgGCLeaveParty); // 4505;
        HANDLE_MSG(k_EMsgGCServerAvailable); // 4506;
        HANDLE_MSG(k_EMsgGCClientConnectToServer); // 4507;
        HANDLE_MSG(k_EMsgGCGameServerInfo); // 4508;
        HANDLE_MSG(k_EMsgGCError); // 4509;
        HANDLE_MSG(k_EMsgGCReplay_UploadedToYouTube); // 4510;
        HANDLE_MSG(k_EMsgGCLANServerAvailable); // 4511;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_Base); // 9100;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchmakingStart); // 9101;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchmakingStop); // 9102;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchmakingClient2ServerPing); // 9103;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate); // 9104;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchmakingServerReservationResponse); // 9106;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchmakingGC2ClientReserve); // 9107;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchmakingClient2GCHello); // 9109;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchmakingGC2ClientHello); // 9110;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchmakingGC2ClientAbandon); // 9112;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchmakingOperator2GCBlogUpdate); // 9117;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ServerNotificationForUserPenalty); // 9118;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientReportPlayer); // 9119;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientReportServer); // 9120;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientCommendPlayer); // 9121;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientReportResponse); // 9122;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientCommendPlayerQuery); // 9123;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientCommendPlayerQueryResponse); // 9124;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_WatchInfoUsers); // 9126;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientRequestPlayersProfile); // 9127;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_PlayersProfile); // 9128;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_PlayerOverwatchCaseUpdate); // 9131;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_PlayerOverwatchCaseAssignment); // 9132;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_PlayerOverwatchCaseStatus); // 9133;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GC2ClientTextMsg); // 9134;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_Client2GCTextMsg); // 9135;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchEndRunRewardDrops); // 9136;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchEndRewardDropsNotification); // 9137;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientRequestWatchInfoFriends2); // 9138;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchList); // 9139;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchListRequestCurrentLiveGames); // 9140;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchListRequestRecentUserGames); // 9141;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GC2ServerReservationUpdate); // 9142;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientVarValueNotificationInfo); // 9144;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchListRequestTournamentGames); // 9146;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchListRequestFullGameInfo); // 9147;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GiftsLeaderboardRequest); // 9148;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GiftsLeaderboardResponse); // 9149;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ServerVarValueNotificationInfo); // 9150;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientSubmitSurveyVote); // 9152;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_Server2GCClientValidate); // 9153;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchListRequestLiveGameForUser); // 9154;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockRequest); // 9156;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_Client2GCEconPreviewDataBlockResponse); // 9157;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_AccountPrivacySettings); // 9158;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_SetMyActivityInfo); // 9159;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchListRequestTournamentPredictions); // 9160;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchListUploadTournamentPredictions); // 9161;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_DraftSummary); // 9162;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientRequestJoinFriendData); // 9163;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientRequestJoinServerData); // 9164;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientRequestNewMission); // 9165;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GC2ClientTournamentInfo); // 9167;
        HANDLE_MSG(k_EMsgGC_GlobalGame_Subscribe); // 9168;
        HANDLE_MSG(k_EMsgGC_GlobalGame_Unsubscribe); // 9169;
        HANDLE_MSG(k_EMsgGC_GlobalGame_Play); // 9170;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_AcknowledgePenalty); // 9171;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_Client2GCRequestPrestigeCoin); // 9172;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GC2ClientGlobalStats); // 9173;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_Client2GCStreamUnlock); // 9174;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_FantasyRequestClientData); // 9175;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_FantasyUpdateClientData); // 9176;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GCToClientSteamdatagramTicket); // 9177;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientToGCRequestTicket); // 9178;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientToGCRequestElevate); // 9179;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GlobalChat); // 9180;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GlobalChat_Subscribe); // 9181;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GlobalChat_Unsubscribe); // 9182;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientAuthKeyCode); // 9183;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GotvSyncPacket); // 9184;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientPlayerDecalSign); // 9185;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientLogonFatalError); // 9187;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientPollState); // 9188;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_Party_Register); // 9189;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_Party_Unregister); // 9190;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_Party_Search); // 9191;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_Party_Invite); // 9192;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_Account_RequestCoPlays); // 9193;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientGCRankUpdate); // 9194;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientRequestOffers); // 9195;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientAccountBalance); // 9196;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientPartyJoinRelay); // 9197;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientPartyWarning); // 9198;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_SetEventFavorite); // 9200;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GetEventFavorites_Request); // 9201;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientPerfReport); // 9202;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GetEventFavorites_Response); // 9203;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientRequestSouvenir); // 9204;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientReportValidation); // 9205;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GC2ClientRefuseSecureMode); // 9206;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GC2ClientRequestValidation); // 9207;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_ClientRedeemMissionReward); // 9209;
        HANDLE_MSG(k_EMsgGCCStrike15_ClientDeepStats); // 9210;
        HANDLE_MSG(k_EMsgGCCStrike15_StartAgreementSessionInGame); // 9211;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GC2ClientInitSystem); // 9212;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_GC2ClientInitSystem_Response); // 9213;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_PrivateQueues); // 9214;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_MatchListTournamentOperatorMgmt); // 9215;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_BetaEnrollment); // 9217;
        HANDLE_MSG(k_EMsgGCCStrike15_v2_SetPlayerLeaderboardSafeName); // 9218;
        HANDLE_MSG(k_EMsgGCBase); // 1000;
        HANDLE_MSG(k_EMsgGCSetItemPosition); // 1001;
        HANDLE_MSG(k_EMsgGCCraft); // 1002;
        HANDLE_MSG(k_EMsgGCCraftResponse); // 1003;
        HANDLE_MSG(k_EMsgGCDelete); // 1004;
        HANDLE_MSG(k_EMsgGCVerifyCacheSubscription); // 1005;
        HANDLE_MSG(k_EMsgGCNameItem); // 1006;
        HANDLE_MSG(k_EMsgGCUnlockCrate); // 1007;
        HANDLE_MSG(k_EMsgGCUnlockCrateResponse); // 1008;
        HANDLE_MSG(k_EMsgGCPaintItem); // 1009;
        HANDLE_MSG(k_EMsgGCPaintItemResponse); // 1010;
        HANDLE_MSG(k_EMsgGCGoldenWrenchBroadcast); // 1011;
        HANDLE_MSG(k_EMsgGCMOTDRequest); // 1012;
        HANDLE_MSG(k_EMsgGCMOTDRequestResponse); // 1013;
        HANDLE_MSG(k_EMsgGCAddItemToSocket_DEPRECATED); // 1014;
        HANDLE_MSG(k_EMsgGCAddItemToSocketResponse_DEPRECATED); // 1015;
        HANDLE_MSG(k_EMsgGCAddSocketToBaseItem_DEPRECATED); // 1016;
        HANDLE_MSG(k_EMsgGCAddSocketToItem_DEPRECATED); // 1017;
        HANDLE_MSG(k_EMsgGCAddSocketToItemResponse_DEPRECATED); // 1018;
        HANDLE_MSG(k_EMsgGCNameBaseItem); // 1019;
        HANDLE_MSG(k_EMsgGCNameBaseItemResponse); // 1020;
        HANDLE_MSG(k_EMsgGCRemoveSocketItem_DEPRECATED); // 1021;
        HANDLE_MSG(k_EMsgGCRemoveSocketItemResponse_DEPRECATED); // 1022;
        HANDLE_MSG(k_EMsgGCCustomizeItemTexture); // 1023;
        HANDLE_MSG(k_EMsgGCCustomizeItemTextureResponse); // 1024;
        HANDLE_MSG(k_EMsgGCUseItemRequest); // 1025;
        HANDLE_MSG(k_EMsgGCUseItemResponse); // 1026;
        HANDLE_MSG(k_EMsgGCGiftedItems_DEPRECATED); // 1027;
        HANDLE_MSG(k_EMsgGCRemoveItemName); // 1030;
        HANDLE_MSG(k_EMsgGCRemoveItemPaint); // 1031;
        HANDLE_MSG(k_EMsgGCGiftWrapItem); // 1032;
        HANDLE_MSG(k_EMsgGCGiftWrapItemResponse); // 1033;
        HANDLE_MSG(k_EMsgGCDeliverGift); // 1034;
        HANDLE_MSG(k_EMsgGCDeliverGiftResponseGiver); // 1035;
        HANDLE_MSG(k_EMsgGCDeliverGiftResponseReceiver); // 1036;
        HANDLE_MSG(k_EMsgGCUnwrapGiftRequest); // 1037;
        HANDLE_MSG(k_EMsgGCUnwrapGiftResponse); // 1038;
        HANDLE_MSG(k_EMsgGCSetItemStyle); // 1039;
        HANDLE_MSG(k_EMsgGCUsedClaimCodeItem); // 1040;
        HANDLE_MSG(k_EMsgGCSortItems); // 1041;
        HANDLE_MSG(k_EMsgGC_RevolvingLootList_DEPRECATED); // 1042;
        HANDLE_MSG(k_EMsgGCLookupAccount); // 1043;
        HANDLE_MSG(k_EMsgGCLookupAccountResponse); // 1044;
        HANDLE_MSG(k_EMsgGCLookupAccountName); // 1045;
        HANDLE_MSG(k_EMsgGCLookupAccountNameResponse); // 1046;
        HANDLE_MSG(k_EMsgGCUpdateItemSchema); // 1049;
        HANDLE_MSG(k_EMsgGCRemoveCustomTexture); // 1051;
        HANDLE_MSG(k_EMsgGCRemoveCustomTextureResponse); // 1052;
        HANDLE_MSG(k_EMsgGCRemoveMakersMark); // 1053;
        HANDLE_MSG(k_EMsgGCRemoveMakersMarkResponse); // 1054;
        HANDLE_MSG(k_EMsgGCRemoveUniqueCraftIndex); // 1055;
        HANDLE_MSG(k_EMsgGCRemoveUniqueCraftIndexResponse); // 1056;
        HANDLE_MSG(k_EMsgGCSaxxyBroadcast); // 1057;
        HANDLE_MSG(k_EMsgGCBackpackSortFinished); // 1058;
        HANDLE_MSG(k_EMsgGCAdjustItemEquippedState); // 1059;
        HANDLE_MSG(k_EMsgGCCollectItem); // 1061;
        HANDLE_MSG(k_EMsgGCItemAcknowledged__DEPRECATED); // 1062;
        HANDLE_MSG(k_EMsgGC_ReportAbuse); // 1065;
        HANDLE_MSG(k_EMsgGC_ReportAbuseResponse); // 1066;
        HANDLE_MSG(k_EMsgGCNameItemNotification); // 1068;
        HANDLE_MSG(k_EMsgGCApplyConsumableEffects); // 1069;
        HANDLE_MSG(k_EMsgGCConsumableExhausted); // 1070;
        HANDLE_MSG(k_EMsgGCShowItemsPickedUp); // 1071;
        HANDLE_MSG(k_EMsgGCClientDisplayNotification); // 1072;
        HANDLE_MSG(k_EMsgGCApplyStrangePart); // 1073;
        HANDLE_MSG(k_EMsgGC_IncrementKillCountAttribute); // 1074;
        HANDLE_MSG(k_EMsgGC_IncrementKillCountResponse); // 1075;
        HANDLE_MSG(k_EMsgGCApplyPennantUpgrade); // 1076;
        HANDLE_MSG(k_EMsgGCSetItemPositions); // 1077;
        HANDLE_MSG(k_EMsgGCApplyEggEssence); // 1078;
        HANDLE_MSG(k_EMsgGCNameEggEssenceResponse); // 1079;
        HANDLE_MSG(k_EMsgGCPaintKitItem); // 1080;
        HANDLE_MSG(k_EMsgGCPaintKitBaseItem); // 1081;
        HANDLE_MSG(k_EMsgGCPaintKitItemResponse); // 1082;
        HANDLE_MSG(k_EMsgGCGiftedItems); // 1083;
        HANDLE_MSG(k_EMsgGCUnlockItemStyle); // 1084;
        HANDLE_MSG(k_EMsgGCUnlockItemStyleResponse); // 1085;
        HANDLE_MSG(k_EMsgGCApplySticker); // 1086;
        HANDLE_MSG(k_EMsgGCItemAcknowledged); // 1087;
        HANDLE_MSG(k_EMsgGCStatTrakSwap); // 1088;
        HANDLE_MSG(k_EMsgGCUserTrackTimePlayedConsecutively); // 1089;
        HANDLE_MSG(k_EMsgGCItemCustomizationNotification); // 1090;
        HANDLE_MSG(k_EMsgGCModifyItemAttribute); // 1091;
        HANDLE_MSG(k_EMsgGCCasketItemAdd); // 1092;
        HANDLE_MSG(k_EMsgGCCasketItemExtract); // 1093;
        HANDLE_MSG(k_EMsgGCCasketItemLoadContents); // 1094;
        HANDLE_MSG(k_EMsgGCTradingBase); // 1500;
        HANDLE_MSG(k_EMsgGCTrading_InitiateTradeRequest); // 1501;
        HANDLE_MSG(k_EMsgGCTrading_InitiateTradeResponse); // 1502;
        HANDLE_MSG(k_EMsgGCTrading_StartSession); // 1503;
        HANDLE_MSG(k_EMsgGCTrading_SetItem); // 1504;
        HANDLE_MSG(k_EMsgGCTrading_RemoveItem); // 1505;
        HANDLE_MSG(k_EMsgGCTrading_UpdateTradeInfo); // 1506;
        HANDLE_MSG(k_EMsgGCTrading_SetReadiness); // 1507;
        HANDLE_MSG(k_EMsgGCTrading_ReadinessResponse); // 1508;
        HANDLE_MSG(k_EMsgGCTrading_SessionClosed); // 1509;
        HANDLE_MSG(k_EMsgGCTrading_CancelSession); // 1510;
        HANDLE_MSG(k_EMsgGCTrading_TradeChatMsg); // 1511;
        HANDLE_MSG(k_EMsgGCTrading_ConfirmOffer); // 1512;
        HANDLE_MSG(k_EMsgGCTrading_TradeTypingChatMsg); // 1513;
        HANDLE_MSG(k_EMsgGCServerBrowser_FavoriteServer); // 1601;
        HANDLE_MSG(k_EMsgGCServerBrowser_BlacklistServer); // 1602;
        HANDLE_MSG(k_EMsgGCServerRentalsBase); // 1700;
        HANDLE_MSG(k_EMsgGCItemPreviewCheckStatus); // 1701;
        HANDLE_MSG(k_EMsgGCItemPreviewStatusResponse); // 1702;
        HANDLE_MSG(k_EMsgGCItemPreviewRequest); // 1703;
        HANDLE_MSG(k_EMsgGCItemPreviewRequestResponse); // 1704;
        HANDLE_MSG(k_EMsgGCItemPreviewExpire); // 1705;
        HANDLE_MSG(k_EMsgGCItemPreviewExpireNotification); // 1706;
        HANDLE_MSG(k_EMsgGCItemPreviewItemBoughtNotification); // 1707;
        HANDLE_MSG(k_EMsgGCDev_NewItemRequest); // 2001;
        HANDLE_MSG(k_EMsgGCDev_NewItemRequestResponse); // 2002;
        HANDLE_MSG(k_EMsgGCDev_PaintKitDropItem); // 2003;
        HANDLE_MSG(k_EMsgGCStoreGetUserData); // 2500;
        HANDLE_MSG(k_EMsgGCStoreGetUserDataResponse); // 2501;
        HANDLE_MSG(k_EMsgGCStorePurchaseInit_DEPRECATED); // 2502;
        HANDLE_MSG(k_EMsgGCStorePurchaseInitResponse_DEPRECATED); // 2503;
        HANDLE_MSG(k_EMsgGCStorePurchaseFinalize); // 2504;
        HANDLE_MSG(k_EMsgGCStorePurchaseFinalizeResponse); // 2505;
        HANDLE_MSG(k_EMsgGCStorePurchaseCancel); // 2506;
        HANDLE_MSG(k_EMsgGCStorePurchaseCancelResponse); // 2507;
        HANDLE_MSG(k_EMsgGCStorePurchaseQueryTxn); // 2508;
        HANDLE_MSG(k_EMsgGCStorePurchaseQueryTxnResponse); // 2509;
        HANDLE_MSG(k_EMsgGCStorePurchaseInit); // 2510;
        HANDLE_MSG(k_EMsgGCStorePurchaseInitResponse); // 2511;
        HANDLE_MSG(k_EMsgGCBannedWordListRequest); // 2512;
        HANDLE_MSG(k_EMsgGCBannedWordListResponse); // 2513;
        HANDLE_MSG(k_EMsgGCToGCBannedWordListBroadcast); // 2514;
        HANDLE_MSG(k_EMsgGCToGCBannedWordListUpdated); // 2515;
        HANDLE_MSG(k_EMsgGCToGCDirtySDOCache); // 2516;
        HANDLE_MSG(k_EMsgGCToGCDirtyMultipleSDOCache); // 2517;
        HANDLE_MSG(k_EMsgGCToGCUpdateSQLKeyValue); // 2518;
        HANDLE_MSG(k_EMsgGCToGCIsTrustedServer); // 2519;
        HANDLE_MSG(k_EMsgGCToGCIsTrustedServerResponse); // 2520;
        HANDLE_MSG(k_EMsgGCToGCBroadcastConsoleCommand); // 2521;
        HANDLE_MSG(k_EMsgGCServerVersionUpdated); // 2522;
        HANDLE_MSG(k_EMsgGCToGCWebAPIAccountChanged); // 2524;
        HANDLE_MSG(k_EMsgGCRequestAnnouncements); // 2525;
        HANDLE_MSG(k_EMsgGCRequestAnnouncementsResponse); // 2526;
        HANDLE_MSG(k_EMsgGCRequestPassportItemGrant); // 2527;
        HANDLE_MSG(k_EMsgGCClientVersionUpdated); // 2528;
        HANDLE_MSG(k_EMsgGCAdjustItemEquippedStateMulti); // 2529;
        HANDLE_MSG(k_EMsgGCRecurringSubscriptionStatus); // 2530;
        HANDLE_MSG(k_EMsgGCAdjustEquipSlots); // 2531;
        HANDLE_MSG(k_EMsgGCClientWelcome); // 4004;
        HANDLE_MSG(k_EMsgGCServerWelcome); // 4005;
        HANDLE_MSG(k_EMsgGCClientHello); // 4006;
        HANDLE_MSG(k_EMsgGCServerHello); // 4007;
        HANDLE_MSG(k_EMsgGCClientConnectionStatus); // 4009;
        HANDLE_MSG(k_EMsgGCServerConnectionStatus); // 4010;
        HANDLE_MSG(k_EMsgGCClientHelloPartner); // 4011;
        HANDLE_MSG(k_EMsgGCClientHelloPW); // 4012;
        HANDLE_MSG(k_EMsgGCClientHelloR2); // 4013;
        HANDLE_MSG(k_EMsgGCClientHelloR3); // 4014;
        HANDLE_MSG(k_EMsgGCClientHelloR4); // 4015;
        HANDLE_MSG(k_EMsgUpdateSessionIP); // 154;
        HANDLE_MSG(k_EMsgRequestSessionIP); // 155;
        HANDLE_MSG(k_EMsgRequestSessionIPResponse); // 156;
    }

    assert(false);
    return "UNKNOWN MESSAGE";
}
