#pragma once

class CCallbackBase;

struct CallbackEvent
{
    int id;
    std::unique_ptr<uint8_t[]> param;
};

struct CallbackListener
{
    CCallbackBase *callback;
    bool removed;
};

class CCallbackMgr
{
public:
    bool SkipSteamRegistration(int id);

    void RegisterCallback(CCallbackBase *callback, int id);
    void UnregisterCallback(CCallbackBase *callback);

    void RunCallbacks(bool gameServer);

    template<typename T>
    void QueueCallback(const T &param, bool gameServer)
    {
        CallbackEvent event{ T::k_iCallback, std::make_unique_for_overwrite<uint8_t[]>(sizeof(T)) };
        memcpy(event.param.get(), &param, sizeof(T));

        auto &events = gameServer ? m_eventsGameServer : m_events;
        events.push_back(std::move(event));
    }

private:
    bool m_runningCallbacks{};

    std::vector<CallbackEvent> m_events;
    std::vector<CallbackEvent> m_eventsGameServer;
    std::vector<CallbackListener> m_listeners;
};
