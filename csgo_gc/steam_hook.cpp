// this file sucks, don't scroll down!!! all you need to know is
// that this is the bridge betweem the game and ClientGC/ServerGC
#include "stdafx.h"
#include "steam_hook.h"
#include "appid.h"
#include "gc_client.h"
#include "gc_server.h"
#include "platform.h"
#include <funchook.h>

struct SteamNetworkingIdentity;

// defines STEAM_PRIVATE_API
#include <steam/steam_api_common.h>

#undef STEAM_PRIVATE_API // we need these public so we can proxy them
#define STEAM_PRIVATE_API(...) __VA_ARGS__

#include <steam/steam_api.h>
#include <steam/steam_gameserver.h>
#include <steam/isteamgamecoordinator.h>

// these should come after steam includes
#include "networking_client.h"
#include "networking_server.h"

// glue for the old interfaces below...
#define STEAM_METHOD_DESC(DESC) STEAM_DESC(DESC)
#define CALL_RESULT(RESULT_TYPE) STEAM_CALL_RESULT(RESULT_TYPE)
class ISteamMasterServerUpdater;
class ISteamUnifiedMessages;
typedef void (*SteamAPI_PostAPIResultInProcess_t)(SteamAPICall_t, void *, uint32, int);

static ISteamClient *s_actualSteamClient;

// FIXME: this can be factored out!!! legacy trauma for global gc insstances
static HSteamPipe s_serverSteamPipe;

// UserStatsReceived_t fails with the new csgo appid, which causes gc callbacks to not run
// to work around this, spoof user stats requests when running under this appid specifically
// we also need to patch serverbrowser to allow for appids over 900...
static void CheckServerBrowserPatch()
{
    static bool attempted = false;

    if (attempted)
    {
        return;
    }

    attempted = true;

    if (AppId::IsOriginal())
    {
        // no need for this patch
        return;
    }

    if (!Platform::PatchServerBrowserAppId(AppId::GetOverride()))
    {
        Platform::Print("serverbrowser patch failed\n");
    }
    else
    {
        Platform::Print("serverbrowser patch succeeded\n");
    }
}

class GCMessageQueue
{
public:
    bool IsMessageAvailable(uint32_t &size)
    {
        if (m_messages.empty())
        {
            return false;
        }

        Message &message = m_messages.front();
        size = static_cast<uint32_t>(message.buffer.size());

        return true;
    }

    bool RetrieveMessage(uint32_t &type, void *buffer, uint32_t bufferSize, uint32_t &size)
    {
        if (m_messages.empty())
        {
            size = 0;
            return false;
        }

        Message &message = m_messages.front();
        type = message.type;
        size = static_cast<uint32_t>(message.buffer.size());

        if (bufferSize < message.buffer.size())
        {
            return false;
        }

        memcpy(buffer, message.buffer.data(), message.buffer.size());
        m_messages.pop();
        return true;
    }

    void AddMessage(uint32_t type, std::vector<uint8_t> &&buffer)
    {
        Message &dest = m_messages.emplace();
        dest.type = type;
        dest.buffer = std::move(buffer);
    }

private:
    struct Message
    {
        uint32_t type{};
        std::vector<uint8_t> buffer;
    };

    std::queue<Message> m_messages;
};

template<typename GC, typename Networking>
class GCWrapper final
{
public:
    template<typename... Args>
    GCWrapper(ISteamNetworkingMessages *networkingMessages, Args &&...args)
        : m_gc{ std::forward<Args>(args)... }
        , m_networking{ networkingMessages }
    {
    }

    GC m_gc;
    Networking m_networking;
    GCMessageQueue m_messageQueue;
};

// these are in file scope for networking, callbacks and gc server
// client connect/disconnect notifications
static GCWrapper<ClientGC, NetworkingClient> *s_clientGC;
static GCWrapper<ServerGC, NetworkingServer> *s_serverGC;

// clunky!!! we need access to this for... some reason
// fetched when s_serverGC is initalized, nulled when it's destroyed
static ISteamGameServer *s_steamGameServer;

static uint64_t GetUserSteamId(HSteamPipe pipe, HSteamUser user)
{
    ISteamUser *interface = s_actualSteamClient->GetISteamUser(user, pipe, STEAMUSER_INTERFACE_VERSION);
    if (!interface)
    {
        Platform::Error("Could not get %s", STEAMUSER_INTERFACE_VERSION);
    }

    CSteamID steamId = interface->GetSteamID();
    assert(steamId.IsValid());
    return steamId.ConvertToUint64();
}

static ISteamNetworkingMessages *GetSteamNetworkingMessages(HSteamPipe pipe, HSteamUser user)
{
    void *interface = s_actualSteamClient->GetISteamGenericInterface(user, pipe, STEAMNETWORKINGMESSAGES_INTERFACE_VERSION);
    if (!interface)
    {
        Platform::Error("Could not get %s", STEAMNETWORKINGMESSAGES_INTERFACE_VERSION);
    }

    return static_cast<ISteamNetworkingMessages *>(interface);
}

static ISteamGameServer *GetSteamGameServer(HSteamPipe pipe, HSteamUser user)
{
    ISteamGameServer *interface = s_actualSteamClient->GetISteamGameServer(user, pipe, STEAMGAMESERVER_INTERFACE_VERSION);
    if (!interface)
    {
        Platform::Error("Could not get %s", STEAMGAMESERVER_INTERFACE_VERSION);
    }

    return interface;
}

// this class sucks but we need to do it this way because
class SteamGameCoordinatorProxy final
{
    const bool m_server;

public:
    SteamGameCoordinatorProxy(HSteamPipe pipe, HSteamUser user)
        : m_server{ pipe == s_serverSteamPipe }
    {
        if (m_server)
        {
            assert(!s_serverGC);
            s_serverGC = new GCWrapper<ServerGC, NetworkingServer>{ GetSteamNetworkingMessages(pipe, user) };

            assert(!s_steamGameServer);
            s_steamGameServer = GetSteamGameServer(pipe, user);
        }
        else
        {
            assert(!s_clientGC);
            s_clientGC = new GCWrapper<ClientGC, NetworkingClient>{ GetSteamNetworkingMessages(pipe, user), GetUserSteamId(pipe, user) };
        }
    }

    ~SteamGameCoordinatorProxy()
    {
        if (m_server)
        {
            assert(s_serverGC);
            delete s_serverGC;
            s_serverGC = nullptr;

            assert(s_steamGameServer);
            s_steamGameServer = nullptr;
        }
        else
        {
            assert(s_clientGC);
            delete s_clientGC;
            s_clientGC = nullptr;
        }
    }

    EGCResults SendMessage(auto, uint32 unMsgType, const void *pubData, uint32 cubData)
    {
        if (m_server)
        {
            assert(s_serverGC);
            s_serverGC->m_gc.PostToGC(GCEvent::Message, unMsgType, pubData, cubData);
        }
        else
        {
            assert(s_clientGC);
            s_clientGC->m_gc.PostToGC(GCEvent::Message, unMsgType, pubData, cubData);
        }

        return k_EGCResultOK;
    }

    bool IsMessageAvailable(auto, uint32 *pcubMsgSize)
    {
        if (m_server)
        {
            return s_serverGC->m_messageQueue.IsMessageAvailable(*pcubMsgSize);
        }
        else
        {
            return s_clientGC->m_messageQueue.IsMessageAvailable(*pcubMsgSize);
        }
    }

    EGCResults RetrieveMessage(auto, uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize)
    {
        bool result;

        if (m_server)
        {
            result = s_serverGC->m_messageQueue.RetrieveMessage(*punMsgType, pubDest, cubDest, *pcubMsgSize);
        }
        else
        {
            result = s_clientGC->m_messageQueue.RetrieveMessage(*punMsgType, pubDest, cubDest, *pcubMsgSize);
        }

        if (!result)
        {
            if (cubDest < *pcubMsgSize)
            {
                return k_EGCResultBufferTooSmall;
            }

            return k_EGCResultNoMessage;
        }

        return k_EGCResultOK;
    }
};

// stupid hack
constexpr SteamAPICall_t CheckSignatureCall = 0x6666666666666666;

// hook so we can spoof the dll signature checks and get rid of the annoying as fuck insecure message box
class SteamUtilsProxy final
{
public:
    bool IsAPICallCompleted(auto original, SteamAPICall_t hSteamAPICall, bool *pbFailed)
    {
        if (hSteamAPICall == CheckSignatureCall)
        {
            if (pbFailed)
            {
                *pbFailed = false;
            }

            return true;
        }

        return original(hSteamAPICall, pbFailed);
    }

    // yeah we won't get here
    //ESteamAPICallFailure GetAPICallFailureReason(auto original, SteamAPICall_t hSteamAPICall)
    //{
    //    if (hSteamAPICall == CheckSignatureCall)
    //    {
    //        // not properly handled, shouldn't get here
    //        assert(false);
    //        return k_ESteamAPICallFailureNone;
    //    }
    //
    //    return original(hSteamAPICall);
    //}

    bool GetAPICallResult(auto original, SteamAPICall_t hSteamAPICall, void *pCallback, int cubCallback, int iCallbackExpected, bool *pbFailed)
    {
        if (hSteamAPICall == CheckSignatureCall
            && cubCallback == sizeof(CheckFileSignature_t)
            && iCallbackExpected == CheckFileSignature_t::k_iCallback)
        {
            if (pbFailed)
            {
                *pbFailed = false;
            }

            CheckFileSignature_t result{};
            result.m_eCheckFileSignature = k_ECheckFileSignatureNoSignaturesFoundForThisApp;
            memcpy(pCallback, &result, sizeof(result));
            return true;
        }

        return original(hSteamAPICall, pCallback, cubCallback, iCallbackExpected, pbFailed);
    }

    SteamAPICall_t CheckFileSignature(auto, const char *)
    {
        // spoof this
        return CheckSignatureCall;
    }
};

static std::vector<UserStatsReceived_t> s_userStatsReceivedCallbacks;

static void QueueUserStatsCallback()
{
    UserStatsReceived_t callback{};
    // m_nGameID not used
    callback.m_eResult = k_EResultOK;
    // m_steamIDUser not used
    s_userStatsReceivedCallbacks.push_back(callback);
}

class SteamUserStatsProxy final
{
public:
    bool RequestCurrentStats(auto original)
    {
        if (!AppId::IsOriginal())
        {
            Platform::Print("Spoofing RequestCurrentStats\n");
            QueueUserStatsCallback();
            return true;
        }

        return original();
    }

    SteamAPICall_t RequestUserStats(auto original, CSteamID steamIDUser)
    {
        if (!AppId::IsOriginal())
        {
            // not used by csgo, but warn anyway
            Platform::Print("RequestUserStats not spoofed!!!\n");
        }

        return original(steamIDUser);
    }
};

class SteamGameServerProxy final
{
public:
    bool InitGameServer(auto original, uint32 unIP, uint16 usGamePort, uint16 usQueryPort, uint32 unFlags, AppId_t nGameAppId, const char *pchVersionString)
    {
        // no longer present in steamworks sdk
        constexpr uint32 k_unServerFlagSecure = 2;

        // never run secure!!!
        unFlags &= ~k_unServerFlagSecure;

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

    void SetGameTags(auto original, const char *pchGameTags)
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

    EBeginAuthSessionResult BeginAuthSession(auto original, const void *pAuthTicket, int cbAuthTicket, CSteamID steamID)
    {
        EBeginAuthSessionResult result = original(pAuthTicket, cbAuthTicket, steamID);
        if (s_serverGC && result == k_EBeginAuthSessionResultOK)
        {
            s_serverGC->m_networking.ClientConnected(steamID.ConvertToUint64(), pAuthTicket, cbAuthTicket);
        }

        return result;
    }

    void EndAuthSession(auto original, CSteamID steamID)
    {
        if (s_serverGC)
        {
            s_serverGC->m_networking.ClientDisconnected(steamID.ConvertToUint64());

            // also remember to unsub from the socache!!! not sure if this does anything in newer builds though
            s_serverGC->m_gc.PostToGC(GCEvent::ClientSOCacheUnsubscribe, steamID.ConvertToUint64(), nullptr, 0);
        }

        original(steamID);
    }
};

class SteamUserProxy final
{
public:
    HAuthTicket GetAuthSessionTicket(auto original, void *pTicket, int cbMaxTicket, uint32 *pcbTicket)
    {
        HAuthTicket ticket = original(pTicket, cbMaxTicket, pcbTicket);
        if (s_clientGC && ticket != k_HAuthTicketInvalid)
        {
            s_clientGC->m_networking.SetAuthTicket(ticket, pTicket, *pcbTicket);
        }

        return ticket;
    }

    HAuthTicket GetAuthSessionTicket(auto original, void *pTicket, int cbMaxTicket, uint32 *pcbTicket, const SteamNetworkingIdentity *pSteamNetworkingIdentity)
    {
        HAuthTicket ticket = original(pTicket, cbMaxTicket, pcbTicket, pSteamNetworkingIdentity);
        if (s_clientGC && ticket != k_HAuthTicketInvalid)
        {
            s_clientGC->m_networking.SetAuthTicket(ticket, pTicket, *pcbTicket);
        }

        return ticket;
    }

    void CancelAuthTicket(auto original, HAuthTicket hAuthTicket)
    {
        if (s_clientGC)
        {
            s_clientGC->m_networking.ClearAuthTicket(hAuthTicket);
        }

        original(hAuthTicket);
    }
};

class SteamMatchmakingServersProxy final
{
public:
    static MatchMakingKeyValuePair_t *ModifyFilters(MatchMakingKeyValuePair_t *pchFilters, uint32 nFilters, std::vector<MatchMakingKeyValuePair_t> &buffer)
    {
        buffer.reserve(nFilters + 1);
        buffer.assign(pchFilters, pchFilters + nFilters);

        if (GetConfig().ShowCsgoGCServersOnly())
        {
            buffer.push_back({ "gametagsand", "csgo_gc" });
        }

        return buffer.data();
    }

    HServerListRequest RequestInternetServerList(auto original,
        AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse)
    {
        CheckServerBrowserPatch();

        std::vector<MatchMakingKeyValuePair_t> buffer;
        MatchMakingKeyValuePair_t *filters = ModifyFilters(*ppchFilters, nFilters, buffer);
        return original(iApp, &filters, buffer.size(), pRequestServersResponse);
    }

    HServerListRequest RequestLANServerList(auto original,
        AppId_t iApp,
        ISteamMatchmakingServerListResponse *pRequestServersResponse)
    {
        CheckServerBrowserPatch();

        return original(iApp, pRequestServersResponse);
    }

    HServerListRequest RequestFriendsServerList(auto original,
        AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse)
    {
        CheckServerBrowserPatch();

        std::vector<MatchMakingKeyValuePair_t> buffer;
        MatchMakingKeyValuePair_t *filters = ModifyFilters(*ppchFilters, nFilters, buffer);
        return original(iApp, &filters, buffer.size(), pRequestServersResponse);
    }

    HServerListRequest RequestFavoritesServerList(auto original,
        AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse)
    {
        CheckServerBrowserPatch();

        std::vector<MatchMakingKeyValuePair_t> buffer;
        MatchMakingKeyValuePair_t *filters = ModifyFilters(*ppchFilters, nFilters, buffer);
        return original(iApp, &filters, buffer.size(), pRequestServersResponse);
    }

    HServerListRequest RequestHistoryServerList(auto original,
        AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse)
    {
        CheckServerBrowserPatch();

        std::vector<MatchMakingKeyValuePair_t> buffer;
        MatchMakingKeyValuePair_t *filters = ModifyFilters(*ppchFilters, nFilters, buffer);
        return original(iApp, &filters, buffer.size(), pRequestServersResponse);
    }

    HServerListRequest RequestSpectatorServerList(auto original,
        AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse)
    {
        CheckServerBrowserPatch();

        std::vector<MatchMakingKeyValuePair_t> buffer;
        MatchMakingKeyValuePair_t *filters = ModifyFilters(*ppchFilters, nFilters, buffer);
        return original(iApp, &filters, buffer.size(), pRequestServersResponse);
    }
};

// now generate the proxy glue...
#include <proxy/steamgamecoordinatorproxy001.h>
#include <proxy/steamgameserverproxy010.h>
#include <proxy/steamgameserverproxy011.h>
#include <proxy/steamgameserverproxy012.h>
#include <proxy/steamgameserverproxy013.h>
#include <proxy/steamgameserverproxy014.h>
#include <proxy/steammatchmakingserversproxy002.h>
#include <proxy/steamuserproxy014.h>
#include <proxy/steamuserproxy015.h>
#include <proxy/steamuserproxy016.h>
#include <proxy/steamuserproxy017.h>
#include <proxy/steamuserproxy018.h>
#include <proxy/steamuserproxy019.h>
#include <proxy/steamuserproxy020.h>
#include <proxy/steamuserproxy021.h>
#include <proxy/steamuserproxy022.h>
#include <proxy/steamuserstatsproxy009.h>
#include <proxy/steamuserstatsproxy010.h>
#include <proxy/steamuserstatsproxy011.h>
#include <proxy/steamuserstatsproxy012.h>
#include <proxy/steamutilsproxy002.h>
#include <proxy/steamutilsproxy005.h>
#include <proxy/steamutilsproxy006.h>
#include <proxy/steamutilsproxy007.h>
#include <proxy/steamutilsproxy008.h>
#include <proxy/steamutilsproxy009.h>
#include <proxy/steamutilsproxy010.h>

template<typename Proxy, typename... Args>
inline Proxy *GetOrCreate(std::unique_ptr<Proxy> &pointer, Args &&...args)
{
    if (!pointer)
    {
        pointer = std::make_unique<Proxy>(std::forward<Args>(args)...);
    }

    return pointer.get();
}

static bool VersionNameIs(const char *version, const char *prefix)
{
    size_t prefixLength = strlen(prefix);
    if (!strncmp(version, prefix, prefixLength))
    {
        if (isdigit(static_cast<unsigned char>(version[prefixLength])))
        {
            return true;
        }
    }

    return false;
}

static bool VersionNumberIs(const char *version, const char *suffix)
{
    return std::string_view{ version }.ends_with(suffix);
}

class SteamInterfaceProxy
{
public:
    SteamInterfaceProxy(HSteamPipe pipe, HSteamUser user)
        : m_steamPipe{ pipe }
        , m_steamUser{ user }
    {
    }

    void *GetInterface(const char *version, void *original)
    {
        auto it = m_interfaces.find(version);
        if (it != m_interfaces.end())
        {
            return it->second.interface;
        }

        // create the interface thunk and shared data on-demand
#define PROXY_INTERFACE(base, number, ...) \
    if (VersionNumberIs(version, #number)) \
    { \
        auto *proxy = new base##Proxy##number{ static_cast<I##base##number *>(original), GetOrCreate(m_proxy##base, ##__VA_ARGS__) }; \
        void *interface = static_cast<I##base##number *>(proxy); \
        m_interfaces.emplace(version, InterfaceProxy{ interface, std::unique_ptr<void, void (*)(void *)>(proxy, [](void *p) { delete static_cast<base##Proxy##number *>(p); }) }); \
        return interface; \
    }

        if (VersionNameIs(version, "SteamGameCoordinator"))
        {
            PROXY_INTERFACE(SteamGameCoordinator, 001, m_steamPipe, m_steamUser);
            Platform::Print("Can't hook %s\n", version);
            return nullptr;
        }

        if (VersionNameIs(version, "SteamGameServer"))
        {
            PROXY_INTERFACE(SteamGameServer, 010);
            PROXY_INTERFACE(SteamGameServer, 011);
            PROXY_INTERFACE(SteamGameServer, 012);
            PROXY_INTERFACE(SteamGameServer, 013);
            PROXY_INTERFACE(SteamGameServer, 014);
            Platform::Print("Can't hook %s\n", version);
            return nullptr;
        }

        if (VersionNameIs(version, "SteamMatchMakingServers"))
        {
            PROXY_INTERFACE(SteamMatchmakingServers, 002);
            Platform::Print("Can't hook %s\n", version);
            return nullptr;
        }

        if (VersionNameIs(version, "SteamUser"))
        {
            PROXY_INTERFACE(SteamUser, 014);
            PROXY_INTERFACE(SteamUser, 015);
            PROXY_INTERFACE(SteamUser, 016);
            PROXY_INTERFACE(SteamUser, 017);
            PROXY_INTERFACE(SteamUser, 018);
            PROXY_INTERFACE(SteamUser, 019);
            PROXY_INTERFACE(SteamUser, 020);
            PROXY_INTERFACE(SteamUser, 021);
            PROXY_INTERFACE(SteamUser, 022);
            Platform::Print("Can't hook %s\n", version);
            return nullptr;
        }

        if (VersionNameIs(version, "STEAMUSERSTATS_INTERFACE_VERSION"))
        {
            PROXY_INTERFACE(SteamUserStats, 009);
            PROXY_INTERFACE(SteamUserStats, 010);
            PROXY_INTERFACE(SteamUserStats, 011);
            PROXY_INTERFACE(SteamUserStats, 012);
            Platform::Print("Can't hook %s\n", version);
            return nullptr;
        }

        if (VersionNameIs(version, "SteamUtils"))
        {
            // old csgo builds fetch SteamUtils with a stale version...
            // technically no need to handle this, but do it for completeness sake
            PROXY_INTERFACE(SteamUtils, 002);

            PROXY_INTERFACE(SteamUtils, 005);
            PROXY_INTERFACE(SteamUtils, 006);
            PROXY_INTERFACE(SteamUtils, 007);
            PROXY_INTERFACE(SteamUtils, 008);
            PROXY_INTERFACE(SteamUtils, 009);
            PROXY_INTERFACE(SteamUtils, 010);
            Platform::Print("Can't hook %s\n", version);
            return nullptr;
        }
#undef PROXY_INTERFACE

        // not proxied
        return nullptr;
    }

private:
    struct InterfaceProxy
    {
        void *interface;
        std::unique_ptr<void, void (*)(void *)> owner;
    };

    HSteamPipe m_steamPipe;
    HSteamUser m_steamUser;

    // steam interface thunks
    std::unordered_map<std::string, InterfaceProxy> m_interfaces;

    // version agnostic proxy data
    std::unique_ptr<SteamGameCoordinatorProxy> m_proxySteamGameCoordinator;
    std::unique_ptr<SteamUtilsProxy> m_proxySteamUtils;
    std::unique_ptr<SteamUserStatsProxy> m_proxySteamUserStats;
    std::unique_ptr<SteamGameServerProxy> m_proxySteamGameServer;
    std::unique_ptr<SteamUserProxy> m_proxySteamUser;
    std::unique_ptr<SteamMatchmakingServersProxy> m_proxySteamMatchmakingServers;
};

class SteamClientProxy final
{
    std::map<uint64_t, SteamInterfaceProxy> m_proxies;

    uint64_t ProxyKey(uint32_t pipe, uint32_t user)
    {
        return static_cast<uint64_t>(user) | (static_cast<uint64_t>(pipe) << 32);
    }

    SteamInterfaceProxy &GetProxy(HSteamPipe pipe, HSteamUser user, [[maybe_unused]] bool allowNoUser)
    {
        assert(pipe);
        assert(user || allowNoUser);

        auto result = m_proxies.try_emplace(ProxyKey(pipe, user), pipe, user);
        return result.first->second;
    }

public:
    ~SteamClientProxy()
    {
        // debug schizo
        assert(m_proxies.empty());
    }

    bool BReleaseSteamPipe(auto original, HSteamPipe hSteamPipe)
    {
        if (hSteamPipe == s_serverSteamPipe)
        {
            s_serverSteamPipe = 0;
        }

        auto lo = m_proxies.lower_bound(ProxyKey(hSteamPipe, 0));
        auto hi = m_proxies.upper_bound(ProxyKey(hSteamPipe, std::numeric_limits<uint32_t>::max()));
        m_proxies.erase(lo, hi);
        return original(hSteamPipe);
    }

    HSteamUser CreateLocalUser(auto original, HSteamPipe *phSteamPipe, EAccountType eAccountType)
    {
        HSteamUser user = original(phSteamPipe, eAccountType);
        if (user && (eAccountType == k_EAccountTypeGameServer || eAccountType == k_EAccountTypeAnonGameServer))
        {
            assert(!s_serverSteamPipe && *phSteamPipe);
            s_serverSteamPipe = *phSteamPipe;
        }

        return user;
    }

    void ReleaseUser(auto original, HSteamPipe hSteamPipe, HSteamUser hUser)
    {
        m_proxies.erase(ProxyKey(hSteamPipe, hUser));
        original(hSteamPipe, hUser);
    }

    template<typename T>
    T *ProxyInterface(T *original, HSteamUser user, HSteamPipe pipe, const char *version, bool allowNoUser = false)
    {
        SteamInterfaceProxy &proxy = GetProxy(pipe, user, allowNoUser);
        T *result = static_cast<T *>(proxy.GetInterface(version, original));
        return result ? result : original;
    }

    ISteamUser *GetISteamUser(auto original, HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion)
    {
        return ProxyInterface(original(hSteamUser, hSteamPipe, pchVersion), hSteamUser, hSteamPipe, pchVersion);
    }

    ISteamGameServer *GetISteamGameServer(auto original, HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion)
    {
        return ProxyInterface(original(hSteamUser, hSteamPipe, pchVersion), hSteamUser, hSteamPipe, pchVersion);
    }

    ISteamUtils *GetISteamUtils(auto original, HSteamPipe hSteamPipe, const char *pchVersion)
    {
        return ProxyInterface(original(hSteamPipe, pchVersion), 0, hSteamPipe, pchVersion, true);
    }

    ISteamMatchmakingServers *GetISteamMatchmakingServers(auto original, HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion)
    {
        return ProxyInterface(original(hSteamUser, hSteamPipe, pchVersion), hSteamUser, hSteamPipe, pchVersion);
    }

    void *GetISteamGenericInterface(auto original, HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion)
    {
        return ProxyInterface(original(hSteamUser, hSteamPipe, pchVersion), hSteamUser, hSteamPipe, pchVersion, true);
    }

    ISteamUserStats *GetISteamUserStats(auto original, HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion)
    {
        return ProxyInterface(original(hSteamUser, hSteamPipe, pchVersion), hSteamUser, hSteamPipe, pchVersion);
    }

    void DestroyAllInterfaces(auto original)
    {
        m_proxies.clear();
        original();
    }
};

static SteamClientProxy s_steamClientProxy;

// steamclient proxy glue
#include <proxy/steamclientproxy010.h>
#include <proxy/steamclientproxy011.h>
#include <proxy/steamclientproxy012.h>
#include <proxy/steamclientproxy013.h>
#include <proxy/steamclientproxy014.h>
#include <proxy/steamclientproxy015.h>
#include <proxy/steamclientproxy016.h>
#include <proxy/steamclientproxy017.h>
#include <proxy/steamclientproxy018.h>
#include <proxy/steamclientproxy019.h>
#include <proxy/steamclientproxy020.h>

using CreateInterface_t = void *(*)(const char *, int *);

static CreateInterface_t Og_CreateInterface;

static void *Hk_CreateInterface(const char *name, int *errorCode)
{
    void *result = Og_CreateInterface(name, errorCode);

    if (VersionNameIs(name, "SteamClient"))
    {
        // this assumes the original pointer won't change, which it shouln't
#define CHECK_STEAMCLIENT(version) \
    if (VersionNumberIs(name, #version)) \
    { \
        static SteamClientProxy##version proxy{ static_cast<ISteamClient##version *>(result), &s_steamClientProxy }; \
        return static_cast<ISteamClient##version *>(&proxy); \
    }
        CHECK_STEAMCLIENT(020)
        CHECK_STEAMCLIENT(019)
        CHECK_STEAMCLIENT(018)
        CHECK_STEAMCLIENT(017)
        CHECK_STEAMCLIENT(016)
        CHECK_STEAMCLIENT(015)
        CHECK_STEAMCLIENT(014)
        CHECK_STEAMCLIENT(013)
        CHECK_STEAMCLIENT(012)
        CHECK_STEAMCLIENT(011)
        CHECK_STEAMCLIENT(010)
#undef CHECK_STEAMCLIENT
        Platform::Print("Can't hook %s\n", name);
    }

    return result;
}

struct CallbackHook
{
    int id;
    CCallbackBase *callback;
};

static bool ShouldHookCallback(int id)
{
    if (id == UserStatsReceived_t::k_iCallback && !AppId::IsOriginal())
    {
        return true;
    }

    // we want to spoof all gc callbacks
    switch (id)
    {
    case GCMessageAvailable_t::k_iCallback:
    case GCMessageFailed_t::k_iCallback:
    case MicroTxnAuthorizationResponse_t::k_iCallback:
        return true;

    default:
        return false;
    }
}

class CallbackAccessor : public CCallbackBase
{
public:
    bool IsGameServer()
    {
        return m_nCallbackFlags & CCallbackBase::k_ECallbackFlagsGameServer;
    }

    void SetRegistered()
    {
        m_nCallbackFlags |= CCallbackBase::k_ECallbackFlagsRegistered;
    }

    void UnsetRegistered()
    {
        m_nCallbackFlags &= ~CCallbackBase::k_ECallbackFlagsRegistered;
    }
};

class CallbackHooks
{
public:
    // returns true if callback was spoofed
    bool RegisterCallback(CCallbackBase *callback, int id)
    {
        if (!ShouldHookCallback(id))
        {
            return false;
        }

        assert((void *)callback != (void *)0xDDDDDDDD);
        CallbackHook callbackHook{ id, callback };
        m_hooks.push_back(callbackHook);

        static_cast<CallbackAccessor *>(callback)->SetRegistered();
        return true;
    }

    // returns true if callback was spoofed
    bool UnregisterCallback(CCallbackBase *callback)
    {
        bool unregistered = false;

        // iterate over all hooks, just in case...
        for (auto it = m_hooks.begin(); it != m_hooks.end();)
        {
            if (it->callback == callback)
            {
                unregistered = true;
                it = m_hooks.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (unregistered)
        {
            static_cast<CallbackAccessor *>(callback)->UnsetRegistered();
            return true;
        }

        return false;
    }

    // runs callbacks matching id immediately
    void RunCallback(bool server, int id, void *param)
    {
        // ported over the schizo list even though csgo_gc doesn't hook such
        // callbacks (ones removing/inserting callbacks during the dispatch)
        for (auto it = m_hooks.begin(); it != m_hooks.end();)
        {
            auto next = std::next(it);

            const CallbackHook &hook = *it;
            bool serverCallback = static_cast<CallbackAccessor *>(hook.callback)->IsGameServer();
            if (server == serverCallback && hook.id == id)
            {
                hook.callback->Run(param);
            }

            it = next;
        }
    }

private:
    std::list<CallbackHook> m_hooks;
};

static CallbackHooks s_callbackHooks;

static void (*Og_SteamAPI_RegisterCallback)(class CCallbackBase *pCallback, int iCallback);
static void (*Og_SteamAPI_UnregisterCallback)(class CCallbackBase *pCallback);
static void (*Og_SteamAPI_RunCallbacks)();
static void (*Og_SteamGameServer_RunCallbacks)();

static void Hk_SteamAPI_RegisterCallback(class CCallbackBase *pCallback, int iCallback)
{
    if (s_callbackHooks.RegisterCallback(pCallback, iCallback))
    {
        return;
    }

    Og_SteamAPI_RegisterCallback(pCallback, iCallback);
}

static void Hk_SteamAPI_UnregisterCallback(class CCallbackBase *pCallback)
{
    if (s_callbackHooks.UnregisterCallback(pCallback))
    {
        return;
    }

    Og_SteamAPI_UnregisterCallback(pCallback);
}

static void Hk_SteamAPI_RunCallbacks()
{
    Og_SteamAPI_RunCallbacks();

    if (s_clientGC)
    {
        std::vector<EventData> events;
        s_clientGC->m_gc.GetHostEvents(events);

        // poll events
        bool runMicroTransactionResponse = false;

        for (EventData &event : events)
        {
            switch (static_cast<HostEvent>(event.type))
            {
            case HostEvent::Message:
                s_clientGC->m_messageQueue.AddMessage(static_cast<uint32_t>(event.id), std::move(event.buffer));
                break;

            case HostEvent::NetMessage:
                s_clientGC->m_networking.SendMessage(event.buffer.data(), static_cast<uint32_t>(event.buffer.size()));
                break;

            case HostEvent::MicroTransactionResponse:
                runMicroTransactionResponse = true;
                break;

            default:
                assert(false);
                break;
            }
        }

        // poll networking
        s_clientGC->m_networking.Update(&s_clientGC->m_gc);

        // run client gc callbacks
        uint32_t messageSize;
        if (s_clientGC->m_messageQueue.IsMessageAvailable(messageSize))
        {
            GCMessageAvailable_t param{};
            param.m_nMessageSize = messageSize;
            s_callbackHooks.RunCallback(false, GCMessageAvailable_t::k_iCallback, &param);
        }

        if (runMicroTransactionResponse)
        {
            Platform::Print("Running MicroTxnAuthorizationResponse_t\n");
            MicroTxnAuthorizationResponse_t response{};
            response.m_bAuthorized = 1; // only field the game cares about
            s_callbackHooks.RunCallback(false, MicroTxnAuthorizationResponse_t::k_iCallback, &response);
        }

        if (s_userStatsReceivedCallbacks.size())
        {
            for (UserStatsReceived_t &data : s_userStatsReceivedCallbacks)
            {
                s_callbackHooks.RunCallback(false, UserStatsReceived_t::k_iCallback, &data);
            }

            s_userStatsReceivedCallbacks.clear();
        }
    }
}

static void Hk_SteamGameServer_RunCallbacks()
{
    Og_SteamGameServer_RunCallbacks();

    if (s_serverGC)
    {
        // only run server gc when logged on as an attempt to more accurately mimic real gc behaviour
        // FIXME: does csgo handle CMsgConnectionStatus?
        assert(s_steamGameServer);
        if (!s_steamGameServer->BLoggedOn())
        {
            return;
        }

        std::vector<EventData> events;
        s_serverGC->m_gc.GetHostEvents(events);

        // poll events
        for (EventData &event : events)
        {
            switch ((HostEvent)event.type)
            {
            case HostEvent::Message:
                s_serverGC->m_messageQueue.AddMessage((uint32_t)event.id, std::move(event.buffer));
                break;

            case HostEvent::NetMessage:
                s_serverGC->m_networking.SendMessage((uint32_t)event.id, event.buffer.data(), static_cast<uint32_t>(event.buffer.size()));
                break;

            default:
                assert(false);
                break;
            }
        }

        // run server gc callbacks
        uint32_t messageSize;
        if (s_serverGC->m_messageQueue.IsMessageAvailable(messageSize))
        {
            GCMessageAvailable_t param{};
            param.m_nMessageSize = messageSize;
            s_callbackHooks.RunCallback(true, GCMessageAvailable_t::k_iCallback, &param);
        }

        SteamNetworkingMessage_t *message;
        while (s_serverGC->m_networking.ReceiveMessage(message))
        {
            s_serverGC->m_gc.PostToGC(GCEvent::NetMessage, message->m_identityPeer.GetSteamID64(), message->GetData(), message->GetSize());
            message->Release();
        }
    }
}

// shows a message box and exits on failure
static void HookCreate(const char *name, void *target, void *hook, void **bridge)
{
    funchook_t *funchook = funchook_create();
    if (!funchook)
    {
        // unlikely (only allocates) but check anyway
        Platform::Error("funchook_create failed for %s", name);
    }

    void *temp = target;
    int result = funchook_prepare(funchook, &temp, hook);
    if (result != 0)
    {
        Platform::Error("funchook_prepare failed for %s: %s", name, funchook_error_message(funchook));
    }

    *bridge = temp;

    result = funchook_install(funchook, 0);
    if (result != 0)
    {
        Platform::Error("funchook_install failed for %s: %s", name, funchook_error_message(funchook));
    }
}

#define INLINE_HOOK(a) HookCreate(#a, reinterpret_cast<void *>(p##a), reinterpret_cast<void *>(Hk_##a), reinterpret_cast<void **>(&Og_##a));

// this is a huge fucking mess, but such is the life of a multiversion steam hook guy

static bool InitializeSteamAPI(void *steamApi, bool dedicated)
{
    if (dedicated)
    {
        using NewInit_t = decltype(SteamInternal_GameServer_Init) *;
        // who knows which variant this dll provides??? this should be the safest choice
        using OldInit_t = bool (*)(uint32, uint16, uint16, uint16, EServerMode, const char *);

        // try the new entry point first
        auto newInit = reinterpret_cast<NewInit_t>(Platform::GetSymbol(steamApi, "SteamInternal_GameServer_Init"));
        if (newInit)
        {
            return newInit(0, 0, 0, STEAMGAMESERVER_QUERY_PORT_SHARED, eServerModeNoAuthentication, "1.38.7.9");
        }

        auto oldInit = reinterpret_cast<OldInit_t>(Platform::GetSymbol(steamApi, "SteamGameServer_Init"));
        if (oldInit)
        {
            return oldInit(0, 0, 0, STEAMGAMESERVER_QUERY_PORT_SHARED, eServerModeNoAuthentication, "1.38.7.9");
        }

        Platform::Error("Could not get SteamGameServer_Init");
    }
    else
    {
        // i think SteamAPI_InitEx is a thing in newer SDKs... not relevant for csgo though
        using Init_t = decltype(SteamAPI_Init) *;

        auto init = reinterpret_cast<Init_t>(Platform::GetSymbol(steamApi, "SteamAPI_Init"));
        if (init)
        {
            return init();
        }

        Platform::Error("Could not get SteamAPI_Init");
    }
}

static void ShutdownSteamAPI(void *steamApi, bool dedicated)
{
    if (dedicated)
    {
        using Shutdown_t = decltype(SteamGameServer_Shutdown) *;

        auto shutdown = reinterpret_cast<Shutdown_t>(Platform::GetSymbol(steamApi, "SteamGameServer_Shutdown"));
        if (shutdown)
        {
            return shutdown();
        }

        Platform::Error("Could not get SteamGameServer_Shutdown");
    }
    else
    {
        using Shutdown_t = decltype(SteamAPI_Shutdown) *;

        auto shutdown = reinterpret_cast<Shutdown_t>(Platform::GetSymbol(steamApi, "SteamAPI_Shutdown"));
        if (shutdown)
        {
            return shutdown();
        }

        Platform::Error("Could not get SteamAPI_Shutdown");
    }
}

void SteamHookInstall(bool dedicated)
{
    // thanks valve for ruining my life
    AppId::Init();

    // no need to write steam_appid.txt, the env var takes precedence
    Platform::SetEnvVar("SteamAppId", std::to_string(AppId::GetOverride()).c_str());

    // load steam api and don't free it so our hooks persist
    void *steamApi = Platform::LoadDynamicLibrary(STEAM_API_LIB);
    if (!steamApi)
    {
        Platform::Error("Could not load steam_api");
    }

    // this is bit of a clusterfuck
    if (!InitializeSteamAPI(steamApi, dedicated))
    {
        // people might not understand what "app 4465480" means, but they
        // already had a hard time understanding this error in general so it's fine
        Platform::Error("Steam initialization failed. Please try the following steps:\n"
                        "- Ensure that Steam is running.\n"
                        "- Restart Steam and try again.\n"
                        "- Verify that you have launched app %u through Steam at least once.",
            AppId::GetOverride());
    }

    uint8_t steamClientPath[4096]; // NOTE: text encoding stored depends on the platform (wchar_t on windows)
    if (!Platform::SteamClientPath(steamClientPath, sizeof(steamClientPath)))
    {
        Platform::Error("Could not get steamclient module path");
    }

    // decrement reference count
    ShutdownSteamAPI(steamApi, dedicated);

    // load steamclient and don't free it so our hooks persist
    void *steamClient = Platform::LoadDynamicLibrary(steamClientPath);
    if (!steamClient)
    {
        Platform::Error("Could not load steamclient");
    }

    auto pCreateInterface = reinterpret_cast<CreateInterface_t>(Platform::GetSymbol(steamClient, "CreateInterface"));
    if (!pCreateInterface)
    {
        Platform::Error("Could not get steamclient factory");
    }

    // get the actual latest steamclient instance, gets used if we want to actually use any steam interfaces
    s_actualSteamClient = static_cast<ISteamClient *>(pCreateInterface(STEAMCLIENT_INTERFACE_VERSION, nullptr));
    if (!s_actualSteamClient)
    {
        Platform::Error("Could not get %s", STEAMCLIENT_INTERFACE_VERSION);
    }

    // see if we should write funchook logs to file
    if (GetConfig().GetLogOutput() == LogOutputFile)
    {
        // same file as gc logs... both will open, append, and close so it's fine
        funchook_set_debug_file("gc_log.txt");
    }

    // hook for steamclient proxy
    INLINE_HOOK(CreateInterface);

#define GET_STEAM_API_FUNC_CHECKED(name) \
    void *p##name = Platform::GetSymbol(steamApi, #name); \
    if (!p##name) \
    { \
        Platform::Error("Could not get %s", #name); \
    }
    GET_STEAM_API_FUNC_CHECKED(SteamAPI_RegisterCallback);
    GET_STEAM_API_FUNC_CHECKED(SteamAPI_UnregisterCallback);
    GET_STEAM_API_FUNC_CHECKED(SteamAPI_RunCallbacks);
    GET_STEAM_API_FUNC_CHECKED(SteamGameServer_RunCallbacks);
#undef GET_STEAM_API_FUNC_CHECKED

    // steam api hooks for gc callbacks
    INLINE_HOOK(SteamAPI_RegisterCallback);
    INLINE_HOOK(SteamAPI_UnregisterCallback);
    INLINE_HOOK(SteamAPI_RunCallbacks);
    INLINE_HOOK(SteamGameServer_RunCallbacks);
}

// these are here for the networking code, just bounce off the trampolines

S_API void S_CALLTYPE SteamAPI_RegisterCallback(class CCallbackBase *pCallback, int iCallback)
{
    return Og_SteamAPI_RegisterCallback(pCallback, iCallback);
}

S_API void S_CALLTYPE SteamAPI_UnregisterCallback(class CCallbackBase *pCallback)
{
    return Og_SteamAPI_UnregisterCallback(pCallback);
}
