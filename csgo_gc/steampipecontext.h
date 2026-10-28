#pragma once

#include <steam/steam_api_common.h>

class CCallbackMgr;
class ISteamClient;
class SteamGameCoordinatorProxy;
class SteamGameServerProxy;
class SteamMatchmakingServersProxy;
class SteamUserProxy;
class SteamUserStatsProxy;
class SteamUtilsProxy;

enum class GCEvent;

bool VersionNameIs(const char *version, const char *prefix);
bool VersionNumberIs(const char *version, const char *suffix);

class SteamPipeContext final : public NonCopyable
{
public:
    SteamPipeContext(HSteamPipe pipe, bool gameServer, ISteamClient &steamClient, CCallbackMgr &callbackMgr);
    ~SteamPipeContext();

    void SetUser(HSteamUser user);
    void ClearUser(HSteamUser user);

    // FIXME: need to revisit these getters...
    HSteamUser GetUser() const { return m_steamUser; }
    bool IsGameServer() const { return m_gameServer; }

    void *GetInterface(const char *version, void *original);

    void RunFrame();

    void PostToGC(GCEvent type, uint64_t id, const void *data, uint32_t size);

private:
    struct InterfaceProxy
    {
        void *interface{};
        std::unique_ptr<void, void (*)(void *)> owner{ nullptr, nullptr };
    };

    InterfaceProxy CreateInterfaceProxy(const char *version, void *original);
    SteamGameCoordinatorProxy *GetOrCreateGameCoordinator();

    const HSteamPipe m_steamPipe;
    const bool m_gameServer;

    ISteamClient &m_steamClient;
    CCallbackMgr &m_callbackMgr;

    // currently one user per pipe, trivial to change later if needed (not)
    HSteamUser m_steamUser{};

    std::unique_ptr<SteamGameCoordinatorProxy> m_proxySteamGameCoordinator;
    std::unique_ptr<SteamUtilsProxy> m_proxySteamUtils;
    std::unique_ptr<SteamUserStatsProxy> m_proxySteamUserStats;
    std::unique_ptr<SteamGameServerProxy> m_proxySteamGameServer;
    std::unique_ptr<SteamUserProxy> m_proxySteamUser;
    std::unique_ptr<SteamMatchmakingServersProxy> m_proxySteamMatchmakingServers;
    std::unordered_map<std::string, InterfaceProxy> m_interfaces;
};
