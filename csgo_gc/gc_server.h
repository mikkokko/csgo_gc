#pragma once

#include "gc_shared.h"

class ServerGC final : public SharedGC
{
public:
    ServerGC();
    ~ServerGC();
    bool RoundMVPMusicKitCountForUserId(int userId, int &musickitmvps) const;

private:
    void HandleEvent(GCEvent type, uint64_t id, const std::vector<uint8_t> &buffer) override;

    // event handlers
    void HandleMessage(uint32_t type, const void *data, uint32_t size);
    void HandleNetMessage(uint64_t steamId, const void *data, uint32_t size);
    void HandleClientSOCacheUnsubscribe(uint64_t steamId);

    void SendServerWelcome();
    void IncrementKillCountAttribute(GCMessageRead &messageRead);
    void UpdateMusicKitMVPState(uint64_t steamId, GCMessageRead &messageRead);

    bool m_sentWelcome{};

    struct MusicKitMVPState
    {
        int userId{};
        uint32_t currentMVPs{};
        bool hasEquippedStatTrakMusicKit{};
    };

    mutable std::mutex m_musicKitMVPStateMutex;
    std::unordered_map<uint64_t, MusicKitMVPState> m_musicKitMVPStateBySteamId;
    std::unordered_map<int, MusicKitMVPState> m_musicKitMVPStateByUserId;
};
