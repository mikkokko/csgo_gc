#pragma once

#include <proxy/steamproxy.h>
#include <steam/isteamuserstats.h>

class CCallbackMgr;

// UserStatsReceived_t fails with the new csgo appid, which causes gc callbacks to not run
// to work around this, spoof user stats requests when running under this appid specifically
class SteamUserStatsProxy final
{
public:
    SteamUserStatsProxy(CCallbackMgr &callbackMgr);

    PROXY_FUNC(bool, RequestCurrentStats);
    PROXY_FUNC(SteamAPICall_t, RequestUserStats, CSteamID steamIDUser);

private:
    CCallbackMgr &m_callbackMgr;
};
