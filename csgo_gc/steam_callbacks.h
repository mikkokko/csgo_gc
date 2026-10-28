#pragma once

class CCallbackBase;

struct CallbackEvent
{
    int id;
    std::unique_ptr<uint8_t[]> param;
};

struct CallbackListener
{
    // nullptr if removal queued
    CCallbackBase *callback;
};

class CCallbackMgr
{
public:
    void RegisterCallback(CCallbackBase *callback, int id);
    void UnregisterCallback(CCallbackBase *callback);

    void RunCallbacks();

    template<typename T>
    void QueueCallback(const T &param)
    {
        CallbackEvent event{ T::k_iCallback, std::make_unique_for_overwrite<uint8_t[]>(sizeof(T)) };
        memcpy(event.param.get(), &param, sizeof(T));
        m_events.push_back(std::move(event));
    }

    static bool SkipSteamRegistration(int id);

    // accessor since the flags are a protected member....
    static bool IsGameServer(const CCallbackBase *callback);

private:
    std::vector<CallbackEvent> m_events;
    std::vector<CallbackListener> m_listeners;
};
