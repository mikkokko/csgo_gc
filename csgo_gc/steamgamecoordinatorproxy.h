#pragma once

#include <proxy/steamproxy.h>
#include <steam/isteamgamecoordinator.h>

class CCallbackMgr;
class ISteamClient;
class SharedGC;

class SteamGameCoordinatorProxy
{
public:
    SteamGameCoordinatorProxy(CCallbackMgr &callbackMgr);
    virtual ~SteamGameCoordinatorProxy();

    virtual SharedGC &GetGC() = 0;
    virtual void RunFrame() = 0;

    PROXY_FUNC(EGCResults, SendMessage, uint32 unMsgType, const void *pubData, uint32 cubData);
    PROXY_FUNC(bool, IsMessageAvailable, uint32 *pcubMsgSize);
    PROXY_FUNC(EGCResults, RetrieveMessage, uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize);

protected:
    void AddMessage(uint32_t type, std::vector<uint8_t> &&buffer);

    CCallbackMgr &m_callbackMgr;

private:
    struct Message
    {
        uint32_t type{};
        std::vector<uint8_t> buffer;
    };

    std::deque<Message> m_messages;
};

// quite a moutful...
std::unique_ptr<SteamGameCoordinatorProxy> CreateSteamGameCoordinatorProxyClient(ISteamClient &steamClient, HSteamPipe pipe, HSteamUser user, CCallbackMgr &callbackMgr);
std::unique_ptr<SteamGameCoordinatorProxy> CreateSteamGameCoordinatorProxyServer(ISteamClient &steamClient, HSteamPipe pipe, HSteamUser user, CCallbackMgr &callbackMgr);
