#pragma once

#include <proxy/steamproxy.h>
#include <steam/isteammatchmaking.h>

class SteamMatchmakingServersProxy final
{
public:
    static MatchMakingKeyValuePair_t *ModifyFilters(MatchMakingKeyValuePair_t *pchFilters, uint32 nFilters, std::vector<MatchMakingKeyValuePair_t> &buffer);

    PROXY_FUNC(HServerListRequest, RequestInternetServerList, AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse);

    PROXY_FUNC(HServerListRequest, RequestLANServerList, AppId_t iApp,
        ISteamMatchmakingServerListResponse *pRequestServersResponse);

    PROXY_FUNC(HServerListRequest, RequestFriendsServerList, AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse);

    PROXY_FUNC(HServerListRequest, RequestFavoritesServerList, AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse);

    PROXY_FUNC(HServerListRequest, RequestHistoryServerList, AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse);

    PROXY_FUNC(HServerListRequest, RequestSpectatorServerList, AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse);
};
