#include "stdafx.h"
#include "steamgameserverproxy.h"
#include "appid.h"
#include "gc_shared.h"
#include "steampipecontext.h"

SteamGameServerProxy::SteamGameServerProxy(SteamPipeContext &context)
    : m_context{ context }
{
}

PROXY_FUNC(bool, SteamGameServerProxy::InitGameServer, uint32 unIP, uint16 usGamePort, uint16 usQueryPort, uint32 unFlags, AppId_t nGameAppId, const char *pchVersionString)
{
    if (AppId::IsOriginal())
    {
        // no longer present in steamworks sdk
        constexpr uint32 k_unServerFlagSecure = 2;

        // never run secure!!!
        unFlags &= ~k_unServerFlagSecure;
    }

    // make sure we're up to date
    pchVersionString = "1.99.9.9";

    // i recall this wasn't used for anything important, but check anyway
    assert(nGameAppId == AppId::GetOverride());

    if (original(unIP, usGamePort, usQueryPort, unFlags, nGameAppId, pchVersionString))
    {
        // add the csgo_gc gametag
        // FIXME: can't do this with steamproxygen!!!
        // why was this done? won't SetGameTags get called immediately after init anyway?
        //m_original->SetGameTags("csgo_gc");
        return true;
    }

    return false;
}

PROXY_FUNC(void, SteamGameServerProxy::SetGameTags, const char *pchGameTags)
{
    std::string tags = pchGameTags;

    if (tags.size())
    {
        tags.append(",csgo_gc");
    }
    else
    {
        tags.append("csgo_gc");
    }

    original(tags.c_str());
}

PROXY_FUNC(EBeginAuthSessionResult, SteamGameServerProxy::BeginAuthSession, const void *pAuthTicket, int cbAuthTicket, CSteamID steamID)
{
    EBeginAuthSessionResult result = original(pAuthTicket, cbAuthTicket, steamID);
    if (result == k_EBeginAuthSessionResultOK)
    {
        m_context.PostToGC(GCEvent::ClientConnected, steamID.ConvertToUint64(), pAuthTicket, cbAuthTicket);
    }

    return result;
}

PROXY_FUNC(void, SteamGameServerProxy::EndAuthSession, CSteamID steamID)
{
    m_context.PostToGC(GCEvent::ClientDisconnected, steamID.ConvertToUint64(), nullptr, 0);
    original(steamID);
}
