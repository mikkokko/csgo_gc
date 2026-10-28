#include "stdafx.h"
#include "steamuserstatsproxy.h"
#include "appid.h"
#include "steam_callbacks.h"

SteamUserStatsProxy::SteamUserStatsProxy(CCallbackMgr &callbackMgr)
    : m_callbackMgr{ callbackMgr }
{
}

PROXY_FUNC(bool, SteamUserStatsProxy::RequestCurrentStats)
{
    if (!AppId::IsOriginal())
    {
        Platform::Print("Spoofing UserStatsReceived_t\n");

        UserStatsReceived_t callback{};
        callback.m_eResult = k_EResultOK; // only field the game cares about
        m_callbackMgr.QueueCallback(callback);
        return true;
    }

    return original();
}

PROXY_FUNC(SteamAPICall_t, SteamUserStatsProxy::RequestUserStats, CSteamID steamIDUser)
{
    if (!AppId::IsOriginal())
    {
        // not used by csgo, but warn anyway
        Platform::Print("RequestUserStats not spoofed!!!\n");
    }

    return original(steamIDUser);
}
