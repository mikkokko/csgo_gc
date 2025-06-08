#include "stdafx.h"
#include "gc_shared.h"

const char *MessageName(uint32_t type);

bool SharedGC::HasOutgoingMessages(uint32_t &size)
{
    if (m_outgoingMessages.empty())
    {
        return false;
    }

    GCMessageWrite &message = m_outgoingMessages.front();
    size = message.Size();

    return true;
}

bool SharedGC::PopOutgoingMessage(uint32_t &type, void *buffer, uint32_t bufferSize, uint32_t &size)
{
    if (m_outgoingMessages.empty())
    {
        size = 0;
        return false;
    }

    GCMessageWrite &message = m_outgoingMessages.front();
    type = message.TypeMasked();
    size = message.Size();

    if (bufferSize < message.Size())
    {
        return false;
    }

    memcpy(buffer, message.Data(), message.Size());
    m_outgoingMessages.pop();
    return true;
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

        HANDLE_MSG(k_ESOMsg_Create);
        HANDLE_MSG(k_ESOMsg_Update);
        HANDLE_MSG(k_ESOMsg_Destroy);
        HANDLE_MSG(k_ESOMsg_CacheSubscribed);
        HANDLE_MSG(k_ESOMsg_CacheUnsubscribed);
        HANDLE_MSG(k_ESOMsg_UpdateMultiple);
        HANDLE_MSG(k_ESOMsg_CacheSubscriptionCheck);
        HANDLE_MSG(k_ESOMsg_CacheSubscriptionRefresh);
    }

    assert(false);
    return "UNKNOWN MESSAGE";
}
