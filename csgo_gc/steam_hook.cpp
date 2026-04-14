// this file sucks, don't scroll down!!! all you need to know is
// that this is the bridge betweem the game and ClientGC/ServerGC
#include "stdafx.h"
#include "steam_hook.h"
#include "appid.h"
#include "gc_client.h"
#include "gc_server.h"
#include "platform.h"
#include <funchook.h>

#ifdef _WIN32
#include <windows.h>
#ifdef SendMessage
#undef SendMessage
#endif
#endif

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

struct PlayerInfo
{
    uint64_t version;
    uint64_t xuid;
    char name[128];
    int userId;
    char guid[33];
    uint32_t friendsId;
    char friendsName[128];
    bool fakePlayer;
    bool isHltv;
    uint32_t customFiles[4];
    unsigned char filesDownloaded;
};

class IGameEvent
{
public:
    virtual ~IGameEvent() = default;
    virtual const char *GetName() const = 0;
    virtual bool IsReliable() const = 0;
    virtual bool IsLocal() const = 0;
    virtual bool IsEmpty(const char *keyName = nullptr) const = 0;
    virtual bool GetBool(const char *keyName = nullptr, bool defaultValue = false) const = 0;
    virtual int GetInt(const char *keyName = nullptr, int defaultValue = 0) const = 0;
    virtual uint64_t GetUint64(const char *keyName = nullptr, uint64_t defaultValue = 0) const = 0;
    virtual float GetFloat(const char *keyName = nullptr, float defaultValue = 0.0f) const = 0;
    virtual const char *GetString(const char *keyName = nullptr, const char *defaultValue = "") const = 0;
    virtual const wchar_t *GetWString(const char *keyName = nullptr, const wchar_t *defaultValue = L"") const = 0;
    virtual const void *GetPtr(const char *keyName = nullptr) const = 0;
    virtual void SetBool(const char *keyName, bool value) = 0;
    virtual void SetInt(const char *keyName, int value) = 0;
};

class IGameEventListener2
{
public:
    virtual ~IGameEventListener2() = default;
    virtual void FireGameEvent(IGameEvent *event) = 0;
    virtual int GetEventDebugID() = 0;
};

class IGameEventManager2
{
public:
    virtual ~IGameEventManager2() = default;
    virtual int LoadEventsFromFile(const char *filename) = 0;
    virtual void Reset() = 0;
    virtual bool AddListener(IGameEventListener2 *listener, const char *name, bool serverSide) = 0;
    virtual bool FindListener(IGameEventListener2 *listener, const char *name) = 0;
    virtual void RemoveListener(IGameEventListener2 *listener) = 0;
};

class IVEngineClient
{
public:
    virtual void Unused0() = 0;
    virtual void Unused1() = 0;
    virtual void Unused2() = 0;
    virtual void Unused3() = 0;
    virtual void Unused4() = 0;
    virtual void Unused5() = 0;
    virtual void Unused6() = 0;
    virtual void Unused7() = 0;
    virtual bool GetPlayerInfo(int entNum, PlayerInfo *info) = 0;
    virtual int GetPlayerForUserID(int userId) = 0;
    virtual void *TextMessageGet(const char *name) = 0;
    virtual bool Con_IsVisible() = 0;
    virtual int GetLocalPlayer() = 0;
};

using CreateInterfaceFn = void *(*)(const char *name, int *returnCode);

constexpr const char *GameEventManagerVersion = "GAMEEVENTSMANAGER002";
constexpr const char *VEngineClientVersion = "VEngineClient014";
constexpr int EventDebugIdInit = 42;

static CreateInterfaceFn s_engineFactory;
static IGameEventManager2 *s_gameEventManager;
static IVEngineClient *s_engineClient;

class ClientGameEventListener final : public IGameEventListener2
{
public:
    void FireGameEvent(IGameEvent *event) override
    {
        if (!event || !s_clientGC || !s_engineClient)
        {
            return;
        }

        const char *eventName = event->GetName();
        if (!strcmp(eventName, "round_start"))
        {
            int localPlayer = s_engineClient->GetLocalPlayer();
            if (localPlayer <= 0)
            {
                return;
            }

            PlayerInfo playerInfo{};
            if (!s_engineClient->GetPlayerInfo(localPlayer, &playerInfo) || playerInfo.userId <= 0)
            {
                return;
            }

            Platform::Print("Listener syncing local music kit state on round_start: userid=%d\n", playerInfo.userId);
            s_clientGC->m_gc.PostToGC(GCEvent::SyncLocalPlayerMusicKitState,
                0,
                &playerInfo.userId,
                sizeof(playerInfo.userId));
            return;
        }

        if (strcmp(eventName, "round_mvp"))
        {
            return;
        }

        int localPlayer = s_engineClient->GetLocalPlayer();
        if (localPlayer <= 0)
        {
            return;
        }

        PlayerInfo playerInfo{};
        if (!s_engineClient->GetPlayerInfo(localPlayer, &playerInfo))
        {
            return;
        }

        if (event->GetInt("userid") != playerInfo.userId)
        {
            return;
        }

        int musickitmvps = event->GetInt("musickitmvps");
        if (musickitmvps <= 0)
        {
            musickitmvps = static_cast<int>(s_clientGC->m_gc.LocalPlayerMusicKitMVPsForRoundMVPEvent());
            if (musickitmvps > 0)
            {
                event->SetInt("musickitmvps", musickitmvps);
                Platform::Print("Listener injected local musickitmvps into round_mvp: %d\n", musickitmvps);
            }
        }

        Platform::Print("Listener saw local round_mvp: userid=%d reason=%d musickitmvps=%d\n",
            playerInfo.userId,
            event->GetInt("reason"),
            musickitmvps);

        s_clientGC->m_gc.PostToGC(GCEvent::SyncLocalPlayerMusicKitState,
            0,
            &playerInfo.userId,
            sizeof(playerInfo.userId));
        s_clientGC->m_gc.PostToGC(GCEvent::LocalPlayerRoundMVP, 0, nullptr, 0);
    }

    int GetEventDebugID() override
    {
        return EventDebugIdInit;
    }
};

class ServerRoundMVPEventListener final : public IGameEventListener2
{
public:
    void FireGameEvent(IGameEvent *event) override
    {
        if (!event || !s_serverGC || strcmp(event->GetName(), "round_mvp"))
        {
            return;
        }

        int userId = event->GetInt("userid");
        if (userId <= 0)
        {
            return;
        }

        int musickitmvps = 0;
        if (!s_serverGC->m_gc.RoundMVPMusicKitCountForUserId(userId, musickitmvps))
        {
            return;
        }

        event->SetInt("musickitmvps", musickitmvps);
        Platform::Print("Server listener injected musickitmvps into round_mvp: userid=%d musickitmvps=%d\n",
            userId,
            musickitmvps);
    }

    int GetEventDebugID() override
    {
        return EventDebugIdInit;
    }
};

static ClientGameEventListener s_clientGameEventListener;
static ServerRoundMVPEventListener s_serverRoundMVPEventListener;
static bool s_clientRoundMVPListenerRegistered = false;
static bool s_clientRoundStartListenerRegistered = false;
static bool s_serverRoundMVPListenerRegistered = false;

static void InitializeClientGameInterfaces()
{
    if (s_engineFactory)
    {
        return;
    }

    s_engineFactory = reinterpret_cast<CreateInterfaceFn>(Platform::ModuleFactory("engine"));
    if (!s_engineFactory)
    {
        Platform::Print("engine CreateInterface not found\n");
        return;
    }

    s_gameEventManager = reinterpret_cast<IGameEventManager2 *>(s_engineFactory(GameEventManagerVersion, nullptr));
    s_engineClient = reinterpret_cast<IVEngineClient *>(s_engineFactory(VEngineClientVersion, nullptr));
}

static void UpdateGameEventListeners()
{
    InitializeClientGameInterfaces();

    if (!s_gameEventManager)
    {
        return;
    }

    // Keep client listener lifecycle tied to the local ClientGC so reconnects do not
    // leave stale listeners behind in engine.dll's event manager.
    if (s_clientGC)
    {
        if (!s_clientRoundMVPListenerRegistered)
        {
            if (s_gameEventManager->AddListener(&s_clientGameEventListener, "round_mvp", false))
            {
                s_clientRoundMVPListenerRegistered = true;
                Platform::Print("Registered round_mvp listener\n");
            }
            else
            {
                Platform::Print("Failed to register round_mvp listener\n");
            }
        }

        if (!s_clientRoundStartListenerRegistered)
        {
            if (s_gameEventManager->AddListener(&s_clientGameEventListener, "round_start", false))
            {
                s_clientRoundStartListenerRegistered = true;
                Platform::Print("Registered round_start listener\n");
            }
            else
            {
                Platform::Print("Failed to register round_start listener\n");
            }
        }
    }
    else
    {
        if (s_clientRoundMVPListenerRegistered || s_clientRoundStartListenerRegistered)
        {
            s_gameEventManager->RemoveListener(&s_clientGameEventListener);
            if (s_clientRoundMVPListenerRegistered)
            {
                Platform::Print("Unregistered round_mvp listener\n");
            }
            if (s_clientRoundStartListenerRegistered)
            {
                Platform::Print("Unregistered round_start listener\n");
            }
            s_clientRoundMVPListenerRegistered = false;
            s_clientRoundStartListenerRegistered = false;
        }
    }

    if (s_serverGC && !s_serverRoundMVPListenerRegistered)
    {
        if (s_gameEventManager->AddListener(&s_serverRoundMVPEventListener, "round_mvp", true))
        {
            s_serverRoundMVPListenerRegistered = true;
            Platform::Print("Registered server-side round_mvp listener\n");
        }
        else
        {
            Platform::Print("Failed to register server-side round_mvp listener\n");
        }
    }
    else if (!s_serverGC && s_serverRoundMVPListenerRegistered)
    {
        s_gameEventManager->RemoveListener(&s_serverRoundMVPEventListener);
        s_serverRoundMVPListenerRegistered = false;
        Platform::Print("Unregistered server-side round_mvp listener\n");
    }
}

template<size_t N>
inline bool InterfaceMatches(const char *name, const char (&compare)[N])
{
    size_t length = strlen(name);
    if (length != (N - 1))
    {
        return false;
    }

    // compare the full version
    if (!memcmp(name, compare, length))
    {
        return true; // matches
    }

    // compare the base name without the last 3 digits
    if (!memcmp(name, compare, length - 3))
    {
        Platform::Print("Got interface version %s, expecting %s\n", name, compare);
        return false; // not a match
    }

    return false; // not a match
}

// this class sucks but we need to do it this way because
class SteamGameCoordinatorProxy final : public ISteamGameCoordinator
{
    const bool m_server;

public:
    SteamGameCoordinatorProxy(uint64_t steamId)
        : m_server{ !steamId }
    {
        if (m_server)
        {
            assert(!s_serverGC);
            s_serverGC = new GCWrapper<ServerGC, NetworkingServer>{ SteamGameServerNetworkingMessages() };
        }
        else
        {
            assert(!s_clientGC);
            s_clientGC = new GCWrapper<ClientGC, NetworkingClient>{ SteamNetworkingMessages(), steamId };
        }
    }

    ~SteamGameCoordinatorProxy()
    {
        if (m_server)
        {
            assert(s_serverGC);
            delete s_serverGC;
            s_serverGC = nullptr;
        }
        else
        {
            assert(s_clientGC);
            delete s_clientGC;
            s_clientGC = nullptr;
        }
    }

    EGCResults SendMessage(uint32 unMsgType, const void *pubData, uint32 cubData) override
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

    bool IsMessageAvailable(uint32 *pcubMsgSize) override
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

    EGCResults RetrieveMessage(uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize) override
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
class SteamUtilsProxy final : public ISteamUtils
{
private:
    ISteamUtils *const m_original;

public:
    SteamUtilsProxy(ISteamUtils *original)
        : m_original{ original }
    {
    }

    uint32 GetSecondsSinceAppActive() override
    {
        return m_original->GetSecondsSinceAppActive();
    }

    uint32 GetSecondsSinceComputerActive() override
    {
        return m_original->GetSecondsSinceComputerActive();
    }

    EUniverse GetConnectedUniverse() override
    {
        return m_original->GetConnectedUniverse();
    }

    uint32 GetServerRealTime() override
    {
        return m_original->GetServerRealTime();
    }

    const char *GetIPCountry() override
    {
        return m_original->GetIPCountry();
    }

    bool GetImageSize(int iImage, uint32 *pnWidth, uint32 *pnHeight) override
    {
        return m_original->GetImageSize(iImage, pnWidth, pnHeight);
    }

    bool GetImageRGBA(int iImage, uint8 *pubDest, int nDestBufferSize) override
    {
        return m_original->GetImageRGBA(iImage, pubDest, nDestBufferSize);
    }

    bool GetCSERIPPort(uint32 *unIP, uint16 *usPort) override
    {
        return m_original->GetCSERIPPort(unIP, usPort);
    }

    uint8 GetCurrentBatteryPower() override
    {
        return m_original->GetCurrentBatteryPower();
    }

    uint32 GetAppID() override
    {
        return m_original->GetAppID();
    }

    void SetOverlayNotificationPosition(ENotificationPosition eNotificationPosition) override
    {
        m_original->SetOverlayNotificationPosition(eNotificationPosition);
    }

    bool IsAPICallCompleted(SteamAPICall_t hSteamAPICall, bool *pbFailed) override
    {
        if (hSteamAPICall == CheckSignatureCall)
        {
            if (pbFailed)
            {
                *pbFailed = false;
            }

            return true;
        }

        return m_original->IsAPICallCompleted(hSteamAPICall, pbFailed);
    }

    ESteamAPICallFailure GetAPICallFailureReason(SteamAPICall_t hSteamAPICall) override
    {
        // yeah we won't get here
        //if (hSteamAPICall == CheckSignatureCall)
        //{
        //    // not properly handled, shouldn't get here
        //    assert(false);
        //    return k_ESteamAPICallFailureNone;
        //}

        return m_original->GetAPICallFailureReason(hSteamAPICall);
    }

    bool GetAPICallResult(SteamAPICall_t hSteamAPICall, void *pCallback, int cubCallback, int iCallbackExpected, bool *pbFailed) override
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

        return m_original->GetAPICallResult(hSteamAPICall, pCallback, cubCallback, iCallbackExpected, pbFailed);
    }

    void RunFrame() override
    {
        m_original->RunFrame();
    }

    uint32 GetIPCCallCount() override
    {
        return m_original->GetIPCCallCount();
    }

    void SetWarningMessageHook(SteamAPIWarningMessageHook_t pFunction) override
    {
        m_original->SetWarningMessageHook(pFunction);
    }

    bool IsOverlayEnabled() override
    {
        return m_original->IsOverlayEnabled();
    }

    bool BOverlayNeedsPresent() override
    {
        return m_original->BOverlayNeedsPresent();
    }

    SteamAPICall_t CheckFileSignature([[maybe_unused]] const char *szFileName) override
    {
        // spoof this
        return CheckSignatureCall;
    }

    bool ShowGamepadTextInput(EGamepadTextInputMode eInputMode, EGamepadTextInputLineMode eLineInputMode, const char *pchDescription, uint32 unCharMax, const char *pchExistingText) override
    {
        return m_original->ShowGamepadTextInput(eInputMode, eLineInputMode, pchDescription, unCharMax, pchExistingText);
    }

    uint32 GetEnteredGamepadTextLength() override
    {
        return m_original->GetEnteredGamepadTextLength();
    }

    bool GetEnteredGamepadTextInput(char *pchText, uint32 cchText) override
    {
        return m_original->GetEnteredGamepadTextInput(pchText, cchText);
    }

    const char *GetSteamUILanguage() override
    {
        return m_original->GetSteamUILanguage();
    }

    bool IsSteamRunningInVR() override
    {
        return m_original->IsSteamRunningInVR();
    }

    void SetOverlayNotificationInset(int nHorizontalInset, int nVerticalInset) override
    {
        m_original->SetOverlayNotificationInset(nHorizontalInset, nVerticalInset);
    }

    bool IsSteamInBigPictureMode() override
    {
        return m_original->IsSteamInBigPictureMode();
    }

    void StartVRDashboard() override
    {
        m_original->StartVRDashboard();
    }

    bool IsVRHeadsetStreamingEnabled() override
    {
        return m_original->IsVRHeadsetStreamingEnabled();
    }

    void SetVRHeadsetStreamingEnabled(bool bEnabled) override
    {
        m_original->SetVRHeadsetStreamingEnabled(bEnabled);
    }

    bool IsSteamChinaLauncher() override
    {
        return m_original->IsSteamChinaLauncher();
    }

    bool InitFilterText(uint32 unFilterOptions) override
    {
        return m_original->InitFilterText(unFilterOptions);
    }

    int FilterText(ETextFilteringContext eContext, CSteamID sourceSteamID, const char *pchInputMessage, char *pchOutFilteredText, uint32 nByteSizeOutFilteredText) override
    {
        return m_original->FilterText(eContext, sourceSteamID, pchInputMessage, pchOutFilteredText, nByteSizeOutFilteredText);
    }

    ESteamIPv6ConnectivityState GetIPv6ConnectivityState(ESteamIPv6ConnectivityProtocol eProtocol) override
    {
        return m_original->GetIPv6ConnectivityState(eProtocol);
    }

    bool IsSteamRunningOnSteamDeck() override
    {
        return m_original->IsSteamRunningOnSteamDeck();
    }

    bool ShowFloatingGamepadTextInput(EFloatingGamepadTextInputMode eKeyboardMode, int nTextFieldXPosition, int nTextFieldYPosition, int nTextFieldWidth, int nTextFieldHeight) override
    {
        return m_original->ShowFloatingGamepadTextInput(eKeyboardMode, nTextFieldXPosition, nTextFieldYPosition, nTextFieldWidth, nTextFieldHeight);
    }

    void SetGameLauncherMode(bool bLauncherMode) override
    {
        m_original->SetGameLauncherMode(bLauncherMode);
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

class SteamUserStatsProxy : public ISteamUserStats
{
    ISteamUserStats *const m_original;

public:
    SteamUserStatsProxy(ISteamUserStats *original)
        : m_original{ original }
    {
    }

    bool RequestCurrentStats() override
    {
        if (!AppId::IsOriginal())
        {
            Platform::Print("Spoofing RequestCurrentStats\n");
            QueueUserStatsCallback();
            return true;
        }

        return m_original->RequestCurrentStats();
    }

    bool GetStat(const char *pchName, int32 *pData) override
    {
        return m_original->GetStat(pchName, pData);
    }

    bool GetStat(const char *pchName, float *pData) override
    {
        return m_original->GetStat(pchName, pData);
    }

    bool SetStat(const char *pchName, int32 nData) override
    {
        return m_original->SetStat(pchName, nData);
    }

    bool SetStat(const char *pchName, float fData) override
    {
        return m_original->SetStat(pchName, fData);
    }

    bool UpdateAvgRateStat(const char *pchName, float flCountThisSession, double dSessionLength) override
    {
        return m_original->UpdateAvgRateStat(pchName, flCountThisSession, dSessionLength);
    }

    bool GetAchievement(const char *pchName, bool *pbAchieved) override
    {
        return m_original->GetAchievement(pchName, pbAchieved);
    }

    bool SetAchievement(const char *pchName) override
    {
        return m_original->SetAchievement(pchName);
    }

    bool ClearAchievement(const char *pchName) override
    {
        return m_original->ClearAchievement(pchName);
    }

    bool GetAchievementAndUnlockTime(const char *pchName, bool *pbAchieved, uint32 *punUnlockTime) override
    {
        return m_original->GetAchievementAndUnlockTime(pchName, pbAchieved, punUnlockTime);
    }

    bool StoreStats() override
    {
        return m_original->StoreStats();
    }

    int GetAchievementIcon(const char *pchName) override
    {
        return m_original->GetAchievementIcon(pchName);
    }

    const char *GetAchievementDisplayAttribute(const char *pchName, const char *pchKey) override
    {
        return m_original->GetAchievementDisplayAttribute(pchName, pchKey);
    }

    bool IndicateAchievementProgress(const char *pchName, uint32 nCurProgress, uint32 nMaxProgress) override
    {
        return m_original->IndicateAchievementProgress(pchName, nCurProgress, nMaxProgress);
    }

    uint32 GetNumAchievements() override
    {
        return m_original->GetNumAchievements();
    }

    const char *GetAchievementName(uint32 iAchievement) override
    {
        return m_original->GetAchievementName(iAchievement);
    }

    SteamAPICall_t RequestUserStats(CSteamID steamIDUser) override
    {
        if (!AppId::IsOriginal())
        {
            // not used by csgo, but warn anyway
            Platform::Print("RequestUserStats not spoofed!!!\n");
        }

        return m_original->RequestUserStats(steamIDUser);
    }

    bool GetUserStat(CSteamID steamIDUser, const char *pchName, int32 *pData) override
    {
        return m_original->GetUserStat(steamIDUser, pchName, pData);
    }

    bool GetUserStat(CSteamID steamIDUser, const char *pchName, float *pData) override
    {
        return m_original->GetUserStat(steamIDUser, pchName, pData);
    }

    bool GetUserAchievement(CSteamID steamIDUser, const char *pchName, bool *pbAchieved) override
    {
        return m_original->GetUserAchievement(steamIDUser, pchName, pbAchieved);
    }

    bool GetUserAchievementAndUnlockTime(CSteamID steamIDUser, const char *pchName, bool *pbAchieved, uint32 *punUnlockTime) override
    {
        return m_original->GetUserAchievementAndUnlockTime(steamIDUser, pchName, pbAchieved, punUnlockTime);
    }

    bool ResetAllStats(bool bAchievementsToo) override
    {
        return m_original->ResetAllStats(bAchievementsToo);
    }

    SteamAPICall_t FindOrCreateLeaderboard(const char *pchLeaderboardName,
        ELeaderboardSortMethod eLeaderboardSortMethod,
        ELeaderboardDisplayType eLeaderboardDisplayType) override
    {
        return m_original->FindOrCreateLeaderboard(pchLeaderboardName, eLeaderboardSortMethod, eLeaderboardDisplayType);
    }

    SteamAPICall_t FindLeaderboard(const char *pchLeaderboardName) override
    {
        return m_original->FindLeaderboard(pchLeaderboardName);
    }

    const char *GetLeaderboardName(SteamLeaderboard_t hSteamLeaderboard) override
    {
        return m_original->GetLeaderboardName(hSteamLeaderboard);
    }

    int GetLeaderboardEntryCount(SteamLeaderboard_t hSteamLeaderboard) override
    {
        return m_original->GetLeaderboardEntryCount(hSteamLeaderboard);
    }

    ELeaderboardSortMethod GetLeaderboardSortMethod(SteamLeaderboard_t hSteamLeaderboard) override
    {
        return m_original->GetLeaderboardSortMethod(hSteamLeaderboard);
    }

    ELeaderboardDisplayType GetLeaderboardDisplayType(SteamLeaderboard_t hSteamLeaderboard) override
    {
        return m_original->GetLeaderboardDisplayType(hSteamLeaderboard);
    }

    SteamAPICall_t DownloadLeaderboardEntries(SteamLeaderboard_t hSteamLeaderboard,
        ELeaderboardDataRequest eLeaderboardDataRequest,
        int nRangeStart,
        int nRangeEnd) override
    {
        return m_original->DownloadLeaderboardEntries(hSteamLeaderboard, eLeaderboardDataRequest, nRangeStart, nRangeEnd);
    }

    SteamAPICall_t DownloadLeaderboardEntriesForUsers(SteamLeaderboard_t hSteamLeaderboard, CSteamID *prgUsers, int cUsers) override
    {
        return m_original->DownloadLeaderboardEntriesForUsers(hSteamLeaderboard, prgUsers, cUsers);
    }

    bool GetDownloadedLeaderboardEntry(SteamLeaderboardEntries_t hSteamLeaderboardEntries,
        int index,
        LeaderboardEntry_t *pLeaderboardEntry,
        int32 *pDetails,
        int cDetailsMax) override
    {
        return m_original->GetDownloadedLeaderboardEntry(hSteamLeaderboardEntries, index, pLeaderboardEntry, pDetails, cDetailsMax);
    }

    SteamAPICall_t UploadLeaderboardScore(SteamLeaderboard_t hSteamLeaderboard,
        ELeaderboardUploadScoreMethod eLeaderboardUploadScoreMethod,
        int32 nScore,
        const int32 *pScoreDetails,
        int cScoreDetailsCount) override
    {
        return m_original->UploadLeaderboardScore(hSteamLeaderboard, eLeaderboardUploadScoreMethod, nScore, pScoreDetails, cScoreDetailsCount);
    }

    SteamAPICall_t AttachLeaderboardUGC(SteamLeaderboard_t hSteamLeaderboard,
        UGCHandle_t hUGC) override
    {
        return m_original->AttachLeaderboardUGC(hSteamLeaderboard, hUGC);
    }

    SteamAPICall_t GetNumberOfCurrentPlayers() override
    {
        return m_original->GetNumberOfCurrentPlayers();
    }

    SteamAPICall_t RequestGlobalAchievementPercentages() override
    {
        return m_original->RequestGlobalAchievementPercentages();
    }

    int GetMostAchievedAchievementInfo(char *pchName, uint32 unNameBufLen,
        float *pflPercent, bool *pbAchieved) override
    {
        return m_original->GetMostAchievedAchievementInfo(pchName, unNameBufLen, pflPercent, pbAchieved);
    }

    int GetNextMostAchievedAchievementInfo(int iIteratorPrevious,
        char *pchName,
        uint32 unNameBufLen,
        float *pflPercent,
        bool *pbAchieved) override
    {
        return m_original->GetNextMostAchievedAchievementInfo(iIteratorPrevious, pchName, unNameBufLen, pflPercent, pbAchieved);
    }

    bool GetAchievementAchievedPercent(const char *pchName, float *pflPercent) override
    {
        return m_original->GetAchievementAchievedPercent(pchName, pflPercent);
    }

    SteamAPICall_t RequestGlobalStats(int nHistoryDays) override
    {
        return m_original->RequestGlobalStats(nHistoryDays);
    }

    bool GetGlobalStat(const char *pchStatName, int64 *pData) override
    {
        return m_original->GetGlobalStat(pchStatName, pData);
    }

    bool GetGlobalStat(const char *pchStatName, double *pData) override
    {
        return m_original->GetGlobalStat(pchStatName, pData);
    }

    int32 GetGlobalStatHistory(const char *pchStatName, int64 *pData, uint32 cubData) override
    {
        return m_original->GetGlobalStatHistory(pchStatName, pData, cubData);
    }

    int32 GetGlobalStatHistory(const char *pchStatName, double *pData, uint32 cubData) override
    {
        return m_original->GetGlobalStatHistory(pchStatName, pData, cubData);
    }

    bool GetAchievementProgressLimits(const char *pchName, int32 *pnMinProgress, int32 *pnMaxProgress) override
    {
        return m_original->GetAchievementProgressLimits(pchName, pnMinProgress, pnMaxProgress);
    }

    bool GetAchievementProgressLimits(const char *pchName, float *pfMinProgress, float *pfMaxProgress) override
    {
        return m_original->GetAchievementProgressLimits(pchName, pfMinProgress, pfMaxProgress);
    }
};

class SteamGameServerProxy final : public ISteamGameServer
{
private:
    ISteamGameServer *m_original;

public:
    SteamGameServerProxy(ISteamGameServer *original)
        : m_original{ original }
    {
    }

    bool InitGameServer(uint32 unIP, uint16 usGamePort, uint16 usQueryPort, uint32 unFlags, AppId_t nGameAppId, const char *pchVersionString) override
    {
        // no longer present in steamworks sdk
        constexpr uint32 k_unServerFlagSecure = 2;

        // never run secure!!!
        unFlags &= ~k_unServerFlagSecure;

        // make sure we're up to date
        pchVersionString = "1.99.9.9";

        // i recall this wasn't used for anything important, but check anyway
        assert(nGameAppId == AppId::GetOverride());

        if (m_original->InitGameServer(unIP, usGamePort, usQueryPort, unFlags, nGameAppId, pchVersionString))
        {
            // add the csgo_gc gametag
            m_original->SetGameTags("csgo_gc");
            return true;
        }

        return false;
    }

    void SetProduct(const char *pszProduct) override
    {
        m_original->SetProduct(pszProduct);
    }

    void SetGameDescription(const char *pszGameDescription) override
    {
        m_original->SetGameDescription(pszGameDescription);
    }

    void SetModDir(const char *pszModDir) override
    {
        m_original->SetModDir(pszModDir);
    }

    void SetDedicatedServer(bool bDedicated) override
    {
        m_original->SetDedicatedServer(bDedicated);
    }

    void LogOn(const char *pszToken) override
    {
        m_original->LogOn(pszToken);
    }

    void LogOnAnonymous() override
    {
        m_original->LogOnAnonymous();
    }

    void LogOff() override
    {
        m_original->LogOff();
    }

    bool BLoggedOn() override
    {
        return m_original->BLoggedOn();
    }

    bool BSecure() override
    {
        return m_original->BSecure();
    }

    CSteamID GetSteamID() override
    {
        return m_original->GetSteamID();
    }

    bool WasRestartRequested() override
    {
        return m_original->WasRestartRequested();
    }

    void SetMaxPlayerCount(int cPlayersMax) override
    {
        m_original->SetMaxPlayerCount(cPlayersMax);
    }

    void SetBotPlayerCount(int cBotplayers) override
    {
        m_original->SetBotPlayerCount(cBotplayers);
    }

    void SetServerName(const char *pszServerName) override
    {
        m_original->SetServerName(pszServerName);
    }

    void SetMapName(const char *pszMapName) override
    {
        m_original->SetMapName(pszMapName);
    }

    void SetPasswordProtected(bool bPasswordProtected) override
    {
        m_original->SetPasswordProtected(bPasswordProtected);
    }

    void SetSpectatorPort(uint16 unSpectatorPort) override
    {
        m_original->SetSpectatorPort(unSpectatorPort);
    }

    void SetSpectatorServerName(const char *pszSpectatorServerName) override
    {
        m_original->SetSpectatorServerName(pszSpectatorServerName);
    }

    void ClearAllKeyValues() override
    {
        m_original->ClearAllKeyValues();
    }

    void SetKeyValue(const char *pKey, const char *pValue) override
    {
        m_original->SetKeyValue(pKey, pValue);
    }

    void SetGameTags(const char *pchGameTags) override
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

        m_original->SetGameTags(tags.c_str());
    }

    void SetGameData(const char *pchGameData) override
    {
        m_original->SetGameData(pchGameData);
    }

    void SetRegion(const char *pszRegion) override
    {
        m_original->SetRegion(pszRegion);
    }

    void SetAdvertiseServerActive(bool bActive) override
    {
        m_original->SetAdvertiseServerActive(bActive);
    }

    HAuthTicket GetAuthSessionTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket) override
    {
        return m_original->GetAuthSessionTicket(pTicket, cbMaxTicket, pcbTicket);
    }

    EBeginAuthSessionResult BeginAuthSession(const void *pAuthTicket, int cbAuthTicket, CSteamID steamID) override
    {
        EBeginAuthSessionResult result = m_original->BeginAuthSession(pAuthTicket, cbAuthTicket, steamID);
        if (s_serverGC && result == k_EBeginAuthSessionResultOK)
        {
            s_serverGC->m_networking.ClientConnected(steamID.ConvertToUint64(), pAuthTicket, cbAuthTicket);
        }

        return result;
    }

    void EndAuthSession(CSteamID steamID) override
    {
        if (s_serverGC)
        {
            s_serverGC->m_networking.ClientDisconnected(steamID.ConvertToUint64());

            // also remember to unsub from the socache!!! not sure if this does anything in newer builds though
            s_serverGC->m_gc.PostToGC(GCEvent::ClientSOCacheUnsubscribe, steamID.ConvertToUint64(), nullptr, 0);
        }

        m_original->EndAuthSession(steamID);
    }

    void CancelAuthTicket(HAuthTicket hAuthTicket) override
    {
        m_original->CancelAuthTicket(hAuthTicket);
    }

    EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID steamID, AppId_t appID) override
    {
        return m_original->UserHasLicenseForApp(steamID, appID);
    }

    bool RequestUserGroupStatus(CSteamID steamIDUser, CSteamID steamIDGroup) override
    {
        return m_original->RequestUserGroupStatus(steamIDUser, steamIDGroup);
    }

    void GetGameplayStats() override
    {
        m_original->GetGameplayStats();
    }

    SteamAPICall_t GetServerReputation() override
    {
        return m_original->GetServerReputation();
    }

    SteamIPAddress_t GetPublicIP() override
    {
        return m_original->GetPublicIP();
    }

    bool HandleIncomingPacket(const void *pData, int cbData, uint32 srcIP, uint16 srcPort) override
    {
        return m_original->HandleIncomingPacket(pData, cbData, srcIP, srcPort);
    }

    int GetNextOutgoingPacket(void *pOut, int cbMaxOut, uint32 *pNetAdr, uint16 *pPort) override
    {
        return m_original->GetNextOutgoingPacket(pOut, cbMaxOut, pNetAdr, pPort);
    }

    SteamAPICall_t AssociateWithClan(CSteamID steamIDClan) override
    {
        return m_original->AssociateWithClan(steamIDClan);
    }

    SteamAPICall_t ComputeNewPlayerCompatibility(CSteamID steamIDNewPlayer) override
    {
        return m_original->ComputeNewPlayerCompatibility(steamIDNewPlayer);
    }

    bool SendUserConnectAndAuthenticate_DEPRECATED(uint32 unIPClient, const void *pvAuthBlob, uint32 cubAuthBlobSize, CSteamID *pSteamIDUser) override
    {
        return m_original->SendUserConnectAndAuthenticate_DEPRECATED(unIPClient, pvAuthBlob, cubAuthBlobSize, pSteamIDUser);
    }

    CSteamID CreateUnauthenticatedUserConnection() override
    {
        return m_original->CreateUnauthenticatedUserConnection();
    }

    void SendUserDisconnect_DEPRECATED(CSteamID steamIDUser) override
    {
        m_original->SendUserDisconnect_DEPRECATED(steamIDUser);
    }

    bool BUpdateUserData(CSteamID steamIDUser, const char *pchPlayerName, uint32 uScore) override
    {
        return m_original->BUpdateUserData(steamIDUser, pchPlayerName, uScore);
    }

    void SetMasterServerHeartbeatInterval_DEPRECATED(int iHeartbeatInterval) override
    {
        m_original->SetMasterServerHeartbeatInterval_DEPRECATED(iHeartbeatInterval);
    }

    void ForceMasterServerHeartbeat_DEPRECATED() override
    {
        m_original->ForceMasterServerHeartbeat_DEPRECATED();
    }
};

class SteamUserProxy : public ISteamUser
{
    ISteamUser *m_original;

public:
    SteamUserProxy(ISteamUser *original)
        : m_original{ original }
    {
    }

    HSteamUser GetHSteamUser() override
    {
        return m_original->GetHSteamUser();
    }

    bool BLoggedOn() override
    {
        return m_original->BLoggedOn();
    }

    CSteamID GetSteamID() override
    {
        return m_original->GetSteamID();
    }

    int InitiateGameConnection_DEPRECATED(void *pAuthBlob, int cbMaxAuthBlob, CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer, bool bSecure) override
    {
        return m_original->InitiateGameConnection_DEPRECATED(pAuthBlob, cbMaxAuthBlob, steamIDGameServer, unIPServer, usPortServer, bSecure);
    }

    void TerminateGameConnection_DEPRECATED(uint32 unIPServer, uint16 usPortServer) override
    {
        m_original->TerminateGameConnection_DEPRECATED(unIPServer, usPortServer);
    }

    void TrackAppUsageEvent(CGameID gameID, int eAppUsageEvent, const char *pchExtraInfo) override
    {
        m_original->TrackAppUsageEvent(gameID, eAppUsageEvent, pchExtraInfo);
    }

    bool GetUserDataFolder(char *pchBuffer, int cubBuffer) override
    {
        return m_original->GetUserDataFolder(pchBuffer, cubBuffer);
    }

    void StartVoiceRecording() override
    {
        m_original->StartVoiceRecording();
    }

    void StopVoiceRecording() override
    {
        m_original->StopVoiceRecording();
    }

    EVoiceResult GetAvailableVoice(uint32 *pcbCompressed, uint32 *pcbUncompressed_Deprecated, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated) override
    {
        return m_original->GetAvailableVoice(pcbCompressed, pcbUncompressed_Deprecated, nUncompressedVoiceDesiredSampleRate_Deprecated);
    }

    EVoiceResult GetVoice(bool bWantCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, bool bWantUncompressed_Deprecated, void *pUncompressedDestBuffer_Deprecated, uint32 cbUncompressedDestBufferSize_Deprecated, uint32 *nUncompressBytesWritten_Deprecated, uint32 nUncompressedVoiceDesiredSampleRate_Deprecated) override
    {
        return m_original->GetVoice(bWantCompressed, pDestBuffer, cbDestBufferSize, nBytesWritten, bWantUncompressed_Deprecated, pUncompressedDestBuffer_Deprecated, cbUncompressedDestBufferSize_Deprecated, nUncompressBytesWritten_Deprecated, nUncompressedVoiceDesiredSampleRate_Deprecated);
    }

    EVoiceResult DecompressVoice(const void *pCompressed, uint32 cbCompressed, void *pDestBuffer, uint32 cbDestBufferSize, uint32 *nBytesWritten, uint32 nDesiredSampleRate) override
    {
        return m_original->DecompressVoice(pCompressed, cbCompressed, pDestBuffer, cbDestBufferSize, nBytesWritten, nDesiredSampleRate);
    }

    uint32 GetVoiceOptimalSampleRate() override
    {
        return m_original->GetVoiceOptimalSampleRate();
    }

    HAuthTicket GetAuthSessionTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket, const SteamNetworkingIdentity *pSteamNetworkingIdentity) override
    {
        HAuthTicket ticket = m_original->GetAuthSessionTicket(pTicket, cbMaxTicket, pcbTicket, pSteamNetworkingIdentity);
        if (s_clientGC && ticket != k_HAuthTicketInvalid)
        {
            s_clientGC->m_networking.SetAuthTicket(ticket, pTicket, *pcbTicket);
        }

        return ticket;
    }

    EBeginAuthSessionResult BeginAuthSession(const void *pAuthTicket, int cbAuthTicket, CSteamID steamID) override
    {
        return m_original->BeginAuthSession(pAuthTicket, cbAuthTicket, steamID);
    }

    void EndAuthSession(CSteamID steamID) override
    {
        m_original->EndAuthSession(steamID);
    }

    void CancelAuthTicket(HAuthTicket hAuthTicket) override
    {
        if (s_clientGC)
        {
            s_clientGC->m_networking.ClearAuthTicket(hAuthTicket);
        }

        m_original->CancelAuthTicket(hAuthTicket);
    }

    EUserHasLicenseForAppResult UserHasLicenseForApp(CSteamID steamID, AppId_t appID) override
    {
        return m_original->UserHasLicenseForApp(steamID, appID);
    }

    bool BIsBehindNAT() override
    {
        return m_original->BIsBehindNAT();
    }

    void AdvertiseGame(CSteamID steamIDGameServer, uint32 unIPServer, uint16 usPortServer) override
    {
        m_original->AdvertiseGame(steamIDGameServer, unIPServer, usPortServer);
    }

    SteamAPICall_t RequestEncryptedAppTicket(void *pDataToInclude, int cbDataToInclude) override
    {
        return m_original->RequestEncryptedAppTicket(pDataToInclude, cbDataToInclude);
    }

    bool GetEncryptedAppTicket(void *pTicket, int cbMaxTicket, uint32 *pcbTicket) override
    {
        return m_original->GetEncryptedAppTicket(pTicket, cbMaxTicket, pcbTicket);
    }

    int GetGameBadgeLevel(int nSeries, bool bFoil) override
    {
        return m_original->GetGameBadgeLevel(nSeries, bFoil);
    }

    int GetPlayerSteamLevel() override
    {
        return m_original->GetPlayerSteamLevel();
    }

    SteamAPICall_t RequestStoreAuthURL(const char *pchRedirectURL) override
    {
        return m_original->RequestStoreAuthURL(pchRedirectURL);
    }

    bool BIsPhoneVerified() override
    {
        return m_original->BIsPhoneVerified();
    }

    bool BIsTwoFactorEnabled() override
    {
        return m_original->BIsTwoFactorEnabled();
    }

    bool BIsPhoneIdentifying() override
    {
        return m_original->BIsPhoneIdentifying();
    }

    bool BIsPhoneRequiringVerification() override
    {
        return m_original->BIsPhoneRequiringVerification();
    }

    SteamAPICall_t GetMarketEligibility() override
    {
        return m_original->GetMarketEligibility();
    }

    SteamAPICall_t GetDurationControl() override
    {
        return m_original->GetDurationControl();
    }

    bool BSetDurationControlOnlineState(EDurationControlOnlineState eNewState) override
    {
        return m_original->BSetDurationControlOnlineState(eNewState);
    }
};

class SteamMatchmakingServersProxy : public ISteamMatchmakingServers
{
    ISteamMatchmakingServers *m_original;

public:
    SteamMatchmakingServersProxy(ISteamMatchmakingServers *original)
        : m_original{ original }
    {
    }

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

    HServerListRequest RequestInternetServerList(AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse) override
    {
        CheckServerBrowserPatch();

        std::vector<MatchMakingKeyValuePair_t> buffer;
        MatchMakingKeyValuePair_t *filters = ModifyFilters(*ppchFilters, nFilters, buffer);
        return m_original->RequestInternetServerList(iApp, &filters, buffer.size(), pRequestServersResponse);
    }

    HServerListRequest RequestLANServerList(AppId_t iApp,
        ISteamMatchmakingServerListResponse *pRequestServersResponse) override
    {
        CheckServerBrowserPatch();

        return m_original->RequestLANServerList(iApp, pRequestServersResponse);
    }

    HServerListRequest RequestFriendsServerList(AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse) override
    {
        CheckServerBrowserPatch();

        std::vector<MatchMakingKeyValuePair_t> buffer;
        MatchMakingKeyValuePair_t *filters = ModifyFilters(*ppchFilters, nFilters, buffer);
        return m_original->RequestFriendsServerList(iApp, &filters, buffer.size(), pRequestServersResponse);
    }

    HServerListRequest RequestFavoritesServerList(AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse) override
    {
        CheckServerBrowserPatch();

        std::vector<MatchMakingKeyValuePair_t> buffer;
        MatchMakingKeyValuePair_t *filters = ModifyFilters(*ppchFilters, nFilters, buffer);
        return m_original->RequestFavoritesServerList(iApp, &filters, buffer.size(), pRequestServersResponse);
    }

    HServerListRequest RequestHistoryServerList(AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse) override
    {
        CheckServerBrowserPatch();

        std::vector<MatchMakingKeyValuePair_t> buffer;
        MatchMakingKeyValuePair_t *filters = ModifyFilters(*ppchFilters, nFilters, buffer);
        return m_original->RequestHistoryServerList(iApp, &filters, buffer.size(), pRequestServersResponse);
    }

    HServerListRequest RequestSpectatorServerList(AppId_t iApp,
        MatchMakingKeyValuePair_t **ppchFilters,
        uint32 nFilters,
        ISteamMatchmakingServerListResponse *pRequestServersResponse) override
    {
        CheckServerBrowserPatch();

        std::vector<MatchMakingKeyValuePair_t> buffer;
        MatchMakingKeyValuePair_t *filters = ModifyFilters(*ppchFilters, nFilters, buffer);
        return m_original->RequestSpectatorServerList(iApp, &filters, buffer.size(), pRequestServersResponse);
    }

    void ReleaseRequest(HServerListRequest hServerListRequest) override
    {
        m_original->ReleaseRequest(hServerListRequest);
    }

    gameserveritem_t *GetServerDetails(HServerListRequest hRequest, int iServer) override
    {
        return m_original->GetServerDetails(hRequest, iServer);
    }

    void CancelQuery(HServerListRequest hRequest) override
    {
        m_original->CancelQuery(hRequest);
    }

    void RefreshQuery(HServerListRequest hRequest) override
    {
        m_original->RefreshQuery(hRequest);
    }

    bool IsRefreshing(HServerListRequest hRequest) override
    {
        return m_original->IsRefreshing(hRequest);
    }

    int GetServerCount(HServerListRequest hRequest) override
    {
        return m_original->GetServerCount(hRequest);
    }

    void RefreshServer(HServerListRequest hRequest, int iServer) override
    {
        m_original->RefreshServer(hRequest, iServer);
    }

    HServerQuery PingServer(uint32 unIP, uint16 usPort, ISteamMatchmakingPingResponse *pRequestServersResponse) override
    {
        return m_original->PingServer(unIP, usPort, pRequestServersResponse);
    }

    HServerQuery PlayerDetails(uint32 unIP, uint16 usPort, ISteamMatchmakingPlayersResponse *pRequestServersResponse) override
    {
        return m_original->PlayerDetails(unIP, usPort, pRequestServersResponse);
    }

    HServerQuery ServerRules(uint32 unIP, uint16 usPort, ISteamMatchmakingRulesResponse *pRequestServersResponse) override
    {
        return m_original->ServerRules(unIP, usPort, pRequestServersResponse);
    }

    void CancelServerQuery(HServerQuery hServerQuery) override
    {
        m_original->CancelServerQuery(hServerQuery);
    }
};

template<typename Interface, typename Proxy, typename... Args>
inline Interface *GetOrCreate(std::unique_ptr<Proxy> &pointer, Args &&...args)
{
    if (!pointer)
    {
        pointer = std::make_unique<Proxy>(std::forward<Args>(args)...);
    }

    return static_cast<Interface *>(pointer.get());
}

class SteamInterfaceProxy
{
public:
    SteamInterfaceProxy(HSteamPipe pipe)
        : m_pipe{ pipe }
    {
    }

    void *GetInterface(const char *version, void *original)
    {
        if (InterfaceMatches(version, STEAMGAMECOORDINATOR_INTERFACE_VERSION))
        {
            // pass 0 as steamid for servers so the wrapper knows it's for a server
            uint64_t steamId = 0;

            if (SteamGameServer_GetHSteamPipe() != m_pipe)
            {
                steamId = SteamUser()->GetSteamID().ConvertToUint64();
            }

            return GetOrCreate<ISteamGameCoordinator>(m_steamGameCoordinator, steamId);
        }
        else if (InterfaceMatches(version, STEAMUTILS_INTERFACE_VERSION))
        {
            return GetOrCreate<ISteamUtils>(m_steamUtils, static_cast<ISteamUtils *>(original));
        }
        else if (InterfaceMatches(version, STEAMUSERSTATS_INTERFACE_VERSION))
        {
            return GetOrCreate<ISteamUserStats>(m_steamUserStats, static_cast<ISteamUserStats *>(original));
        }
        else if (InterfaceMatches(version, STEAMGAMESERVER_INTERFACE_VERSION))
        {
            return GetOrCreate<ISteamGameServer>(m_steamGameServer, static_cast<ISteamGameServer *>(original));
        }
        else if (InterfaceMatches(version, STEAMUSER_INTERFACE_VERSION))
        {
            return GetOrCreate<ISteamUser>(m_steamUser, static_cast<ISteamUser *>(original));
        }
        else if (InterfaceMatches(version, STEAMMATCHMAKINGSERVERS_INTERFACE_VERSION))
        {
            return GetOrCreate<ISteamMatchmakingServers>(m_steamMatchmakingServers, static_cast<ISteamMatchmakingServers *>(original));
        }

        return nullptr;
    }

private:
    const HSteamPipe m_pipe;

    std::unique_ptr<SteamGameCoordinatorProxy> m_steamGameCoordinator;
    std::unique_ptr<SteamUtilsProxy> m_steamUtils;
    std::unique_ptr<SteamUserStatsProxy> m_steamUserStats;
    std::unique_ptr<SteamGameServerProxy> m_steamGameServer;
    std::unique_ptr<SteamUserProxy> m_steamUser;
    std::unique_ptr<SteamMatchmakingServersProxy> m_steamMatchmakingServers;
};

class SteamClientProxy : public ISteamClient
{
    ISteamClient *m_original{};
    std::unordered_map<uint64_t, SteamInterfaceProxy> m_proxies;

    uint64_t ProxyKey(HSteamPipe pipe, HSteamUser user)
    {
        return static_cast<uint64_t>(pipe) | (static_cast<uint64_t>(user) << 32);
    }

    SteamInterfaceProxy &GetProxy(HSteamPipe pipe, HSteamUser user, [[maybe_unused]] bool allowNoUser)
    {
        assert(pipe);
        assert(user || allowNoUser);

        auto result = m_proxies.try_emplace(ProxyKey(pipe, user), pipe);
        return result.first->second;
    }

    void RemoveProxy(HSteamPipe pipe, HSteamUser user)
    {
        auto it = m_proxies.find(ProxyKey(pipe, user));
        if (it != m_proxies.end())
        {
            m_proxies.erase(it);
        }
        else
        {
            assert(false);
        }
    }

public:
    void SetOriginal(ISteamClient *original)
    {
        assert(!m_original || m_original == original);
        m_original = original;
    }

    ~SteamClientProxy()
    {
        // debug schizo
        assert(m_proxies.empty());
    }

    HSteamPipe CreateSteamPipe() override
    {
        return m_original->CreateSteamPipe();
    }

    bool BReleaseSteamPipe(HSteamPipe hSteamPipe) override
    {
        // remove proxies not tied to a specific user, e.g. ISteamUtils
        RemoveProxy(hSteamPipe, 0);

        return m_original->BReleaseSteamPipe(hSteamPipe);
    }

    HSteamUser ConnectToGlobalUser(HSteamPipe hSteamPipe) override
    {
        return m_original->ConnectToGlobalUser(hSteamPipe);
    }

    HSteamUser CreateLocalUser(HSteamPipe *phSteamPipe, EAccountType eAccountType) override
    {
        return m_original->CreateLocalUser(phSteamPipe, eAccountType);
    }

    void ReleaseUser(HSteamPipe hSteamPipe, HSteamUser hUser) override
    {
        RemoveProxy(hSteamPipe, hUser);

        m_original->ReleaseUser(hSteamPipe, hUser);
    }

    template<typename T>
    T *ProxyInterface(T *original, HSteamUser user, HSteamPipe pipe, const char *version, bool allowNoUser = false)
    {
        SteamInterfaceProxy &proxy = GetProxy(pipe, user, allowNoUser);
        T *result = static_cast<T *>(proxy.GetInterface(version, original));
        return result ? result : original;
    }

    // temp macro
#define PROXY_INTERFACE(func, user, pipe, version, ...) ProxyInterface(m_original->func(user, pipe, version), user, pipe, version, ##__VA_ARGS__)

    ISteamUser *GetISteamUser(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamUser, hSteamUser, hSteamPipe, pchVersion);
    }

    ISteamGameServer *GetISteamGameServer(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamGameServer, hSteamUser, hSteamPipe, pchVersion);
    }

    void SetLocalIPBinding(const SteamIPAddress_t &unIP, uint16 usPort) override
    {
        m_original->SetLocalIPBinding(unIP, usPort);
    }

    ISteamFriends *GetISteamFriends(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamFriends, hSteamUser, hSteamPipe, pchVersion);
    }

    ISteamUtils *GetISteamUtils(HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return ProxyInterface(m_original->GetISteamUtils(hSteamPipe, pchVersion), 0, hSteamPipe, pchVersion, true);
    }

    ISteamMatchmaking *GetISteamMatchmaking(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamMatchmaking, hSteamUser, hSteamPipe, pchVersion);
    }

    ISteamMatchmakingServers *GetISteamMatchmakingServers(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamMatchmakingServers, hSteamUser, hSteamPipe, pchVersion);
    }

    void *GetISteamGenericInterface(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamGenericInterface, hSteamUser, hSteamPipe, pchVersion, true);
    }

    ISteamUserStats *GetISteamUserStats(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamUserStats, hSteamUser, hSteamPipe, pchVersion);
    }

    ISteamGameServerStats *GetISteamGameServerStats(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamGameServerStats, hSteamuser, hSteamPipe, pchVersion);
    }

    ISteamApps *GetISteamApps(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamApps, hSteamUser, hSteamPipe, pchVersion);
    }

    ISteamNetworking *GetISteamNetworking(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamNetworking, hSteamUser, hSteamPipe, pchVersion);
    }

    ISteamRemoteStorage *GetISteamRemoteStorage(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamRemoteStorage, hSteamuser, hSteamPipe, pchVersion);
    }

    ISteamScreenshots *GetISteamScreenshots(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamScreenshots, hSteamuser, hSteamPipe, pchVersion);
    }

    ISteamGameSearch *GetISteamGameSearch(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamGameSearch, hSteamuser, hSteamPipe, pchVersion);
    }

    void RunFrame() override
    {
        m_original->RunFrame();
    }

    uint32 GetIPCCallCount() override
    {
        return m_original->GetIPCCallCount();
    }

    void SetWarningMessageHook(SteamAPIWarningMessageHook_t pFunction) override
    {
        m_original->SetWarningMessageHook(pFunction);
    }

    bool BShutdownIfAllPipesClosed() override
    {
        return m_original->BShutdownIfAllPipesClosed();
    }

    ISteamHTTP *GetISteamHTTP(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamHTTP, hSteamuser, hSteamPipe, pchVersion);
    }

    void *DEPRECATED_GetISteamUnifiedMessages(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(DEPRECATED_GetISteamUnifiedMessages, hSteamuser, hSteamPipe, pchVersion);
    }

    ISteamController *GetISteamController(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamController, hSteamUser, hSteamPipe, pchVersion);
    }

    ISteamUGC *GetISteamUGC(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamUGC, hSteamUser, hSteamPipe, pchVersion);
    }

    ISteamAppList *GetISteamAppList(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamAppList, hSteamUser, hSteamPipe, pchVersion);
    }

    ISteamMusic *GetISteamMusic(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamMusic, hSteamuser, hSteamPipe, pchVersion);
    }

    ISteamMusicRemote *GetISteamMusicRemote(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamMusicRemote, hSteamuser, hSteamPipe, pchVersion);
    }

    ISteamHTMLSurface *GetISteamHTMLSurface(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamHTMLSurface, hSteamuser, hSteamPipe, pchVersion);
    }

    void DEPRECATED_Set_SteamAPI_CPostAPIResultInProcess(void (*func)()) override
    {
        m_original->DEPRECATED_Set_SteamAPI_CPostAPIResultInProcess(func);
    }

    void DEPRECATED_Remove_SteamAPI_CPostAPIResultInProcess(void (*func)()) override
    {
        m_original->DEPRECATED_Remove_SteamAPI_CPostAPIResultInProcess(func);
    }

    void Set_SteamAPI_CCheckCallbackRegisteredInProcess(SteamAPI_CheckCallbackRegistered_t func) override
    {
        m_original->Set_SteamAPI_CCheckCallbackRegisteredInProcess(func);
    }

    ISteamInventory *GetISteamInventory(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamInventory, hSteamuser, hSteamPipe, pchVersion);
    }

    ISteamVideo *GetISteamVideo(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamVideo, hSteamuser, hSteamPipe, pchVersion);
    }

    ISteamParentalSettings *GetISteamParentalSettings(HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamParentalSettings, hSteamuser, hSteamPipe, pchVersion);
    }

    ISteamInput *GetISteamInput(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamInput, hSteamUser, hSteamPipe, pchVersion);
    }

    ISteamParties *GetISteamParties(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamParties, hSteamUser, hSteamPipe, pchVersion);
    }

    ISteamRemotePlay *GetISteamRemotePlay(HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override
    {
        return PROXY_INTERFACE(GetISteamRemotePlay, hSteamUser, hSteamPipe, pchVersion);
    }

    void DestroyAllInterfaces() override
    {
        m_proxies.clear();
        m_original->DestroyAllInterfaces();
    }
};

static SteamClientProxy s_steamClientProxy;

static void *(*Og_CreateInterface)(const char *, int *errorCode);

static void *Hk_CreateInterface(const char *name, int *errorCode)
{
    void *result = Og_CreateInterface(name, errorCode);

    if (InterfaceMatches(name, STEAMCLIENT_INTERFACE_VERSION))
    {
        s_steamClientProxy.SetOriginal(static_cast<ISteamClient *>(result));
        return &s_steamClientProxy;
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
        for (const CallbackHook &hook : m_hooks)
        {
            bool serverCallback = static_cast<CallbackAccessor *>(hook.callback)->IsGameServer();
            if (server == serverCallback && hook.id == id)
            {
                hook.callback->Run(param);
            }
        }
    }

private:
    std::vector<CallbackHook> m_hooks;
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

    UpdateGameEventListeners();

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

    UpdateGameEventListeners();

    if (s_serverGC)
    {
        // only run server gc when logged on as an attempt to more accurately mimic real gc behaviour
        // FIXME: does csgo handle CMsgConnectionStatus?
        if (!SteamGameServer()->BLoggedOn())
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

#define INLINE_HOOK(a) HookCreate(#a, reinterpret_cast<void *>(a), reinterpret_cast<void *>(Hk_##a), reinterpret_cast<void **>(&Og_##a));

static bool InitializeSteamAPI(bool dedicated)
{
    if (dedicated)
    {
        return SteamGameServer_Init(0, 0, STEAMGAMESERVER_QUERY_PORT_SHARED, eServerModeNoAuthentication, "1.38.7.9");
    }
    else
    {
        return SteamAPI_Init();
    }
}

static void ShutdownSteamAPI(bool dedicated)
{
    if (dedicated)
    {
        SteamGameServer_Shutdown();
    }
    else
    {
        SteamAPI_Shutdown();
    }
}

void SteamHookInstall(bool dedicated)
{
    // thanks valve for ruining my life
    AppId::Init();

    // no need to write steam_appid.txt, the env var takes precedence
    Platform::SetEnvVar("SteamAppId", std::to_string(AppId::GetOverride()).c_str());

    // this is bit of a clusterfuck
    if (!InitializeSteamAPI(dedicated))
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
    ShutdownSteamAPI(dedicated);

    // load steamclient
    void *CreateInterface = Platform::SteamClientFactory(steamClientPath);
    if (!CreateInterface)
    {
        Platform::Error("Could not get steamclient factory");
    }

    INLINE_HOOK(CreateInterface);

    // steam api hooks for gc callbacks
    INLINE_HOOK(SteamAPI_RegisterCallback);
    INLINE_HOOK(SteamAPI_UnregisterCallback);
    INLINE_HOOK(SteamAPI_RunCallbacks);
    INLINE_HOOK(SteamGameServer_RunCallbacks);
}
