#include "stdafx.h"
#include "steamuserproxy.h"
#include "gc_shared.h"
#include "steampipecontext.h"

SteamUserProxy::SteamUserProxy(SteamPipeContext &context)
    : m_context{ context }
{
}

PROXY_FUNC(HAuthTicket, SteamUserProxy::GetAuthSessionTicket, void *pTicket, int cbMaxTicket, uint32 *pcbTicket)
{
    HAuthTicket ticket = original(pTicket, cbMaxTicket, pcbTicket);
    if (ticket != k_HAuthTicketInvalid)
    {
        m_context.PostToGC(GCEvent::SetAuthTicket, ticket, pTicket, *pcbTicket);
    }

    return ticket;
}

PROXY_FUNC(HAuthTicket, SteamUserProxy::GetAuthSessionTicket, void *pTicket, int cbMaxTicket, uint32 *pcbTicket, const SteamNetworkingIdentity *pSteamNetworkingIdentity)
{
    HAuthTicket ticket = original(pTicket, cbMaxTicket, pcbTicket, pSteamNetworkingIdentity);
    if (ticket != k_HAuthTicketInvalid)
    {
        m_context.PostToGC(GCEvent::SetAuthTicket, ticket, pTicket, *pcbTicket);
    }

    return ticket;
}

PROXY_FUNC(void, SteamUserProxy::CancelAuthTicket, HAuthTicket hAuthTicket)
{
    m_context.PostToGC(GCEvent::ClearAuthTicket, hAuthTicket, nullptr, 0);
    original(hAuthTicket);
}
