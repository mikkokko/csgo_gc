#include "stdafx.h"
#include "steam_callbacks.h"
#include "appid.h"

struct SteamNetworkingIdentity;

#include <steam/steam_api_common.h>

#include <steam/isteamgamecoordinator.h>
#include <steam/isteamuser.h>
#include <steam/isteamuserstats.h>

void CCallbackMgr::RegisterCallback(CCallbackBase *callback, int id)
{
    if (SkipSteamRegistration(id))
    {
        // we are responsible for initalization
        callback->m_nCallbackFlags |= CCallbackBase::k_ECallbackFlagsRegistered;
        callback->m_iCallback = id;
    }

    m_listeners.emplace_back(callback);
}

void CCallbackMgr::UnregisterCallback(CCallbackBase *callback)
{
    if (!(callback->m_nCallbackFlags & CCallbackBase::k_ECallbackFlagsRegistered))
    {
        return;
    }

    if (SkipSteamRegistration(callback->m_iCallback))
    {
        // we are responsible for deinitalization
        callback->m_nCallbackFlags &= ~CCallbackBase::k_ECallbackFlagsRegistered;
    }

    for (CallbackListener &listener : m_listeners)
    {
        if (listener.callback == callback)
        {
            listener.callback = nullptr;
        }
    }
}

bool CCallbackMgr::SkipSteamRegistration(int id)
{
    if (id == UserStatsReceived_t::k_iCallback && !AppId::IsOriginal())
    {
        return true;
    }

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

void CCallbackMgr::RunCallbacks(bool gameServer)
{
    auto &events = gameServer ? m_eventsGameServer : m_events;
    size_t eventCount = events.size();

    for (size_t i = 0; i < eventCount; i++)
    {
        CallbackEvent event = std::move(events[i]);

        size_t listenerCount = m_listeners.size();
        for (size_t j = 0; j < listenerCount; j++)
        {
            CallbackListener listener = m_listeners[j];
            if (!listener.callback)
            {
                // no longer presenet
                continue;
            }

            CCallbackBase *callback = listener.callback;
            bool isGameServer = (callback->m_nCallbackFlags & CCallbackBase::k_ECallbackFlagsGameServer) != 0;

            if (callback->m_iCallback == event.id && isGameServer == gameServer)
            {
                callback->Run(event.param.get());
            }
        }
    }

    events.erase(events.begin(), events.begin() + eventCount);

    std::erase_if(m_listeners, [](const auto &listener)
        { return !listener.callback; });
}
