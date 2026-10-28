#pragma once

#include <proxy/steamproxy.h>
#include <steam/isteamgameserver.h>

class SteamPipeContext;

class SteamGameServerProxy final
{
public:
    SteamGameServerProxy(SteamPipeContext &context);

    PROXY_FUNC(bool, InitGameServer, uint32 unIP, uint16 usGamePort, uint16 usQueryPort, uint32 unFlags, AppId_t nGameAppId, const char *pchVersionString);
    PROXY_FUNC(void, SetGameTags, const char *pchGameTags);
    PROXY_FUNC(EBeginAuthSessionResult, BeginAuthSession, const void *pAuthTicket, int cbAuthTicket, CSteamID steamID);
    PROXY_FUNC(void, EndAuthSession, CSteamID steamID);

private:
    SteamPipeContext &m_context;
};
