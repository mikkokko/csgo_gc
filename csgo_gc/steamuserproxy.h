#pragma once

#include <proxy/steamproxy.h>

struct SteamNetworkingIdentity;

#include <steam/isteamuser.h>

class SteamPipeContext;

class SteamUserProxy final
{
public:
    SteamUserProxy(SteamPipeContext &context);

    PROXY_FUNC(HAuthTicket, GetAuthSessionTicket, void *pTicket, int cbMaxTicket, uint32 *pcbTicket);
    PROXY_FUNC(HAuthTicket, GetAuthSessionTicket, void *pTicket, int cbMaxTicket, uint32 *pcbTicket, const SteamNetworkingIdentity *pSteamNetworkingIdentity);
    PROXY_FUNC(void, CancelAuthTicket, HAuthTicket hAuthTicket);

private:
    SteamPipeContext &m_context;
};
