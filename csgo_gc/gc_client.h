#pragma once

#include "config.h"
#include "gc_message.h"
#include "inventory.h"
#include "networking_client.h"

class ClientGC
{
public:
    ClientGC(uint64_t steamId);
    ~ClientGC();

    void HandleMessage(uint32_t type, const void *data, uint32_t size);
    bool HasOutgoingMessages(uint32_t &size);
    bool PopOutgoingMessage(uint32_t &type, void *buffer, uint32_t bufferSize, uint32_t &size);

    void Update();

    // called from net code
    void SendSOCacheToGameSever();
    void AddOutgoingMessage(const void *data, uint32_t size);

private:
    void OnClientHello(const void *data, uint32_t size);
    void AdjustItemEquippedState(const void *data, uint32_t size);
    void ClientPlayerDecalSign(const void *data, uint32_t size);
    void UseItemRequest(const void *data, uint32_t size);
    void ClientRequestJoinServerData(const void *data, uint32_t size);
    void SetItemPositions(const void *data, uint32_t size);
    void IncrementKillCountAttribute(const void *data, uint32_t size);
    void ApplySticker(const void *data, uint32_t size);

    void UnlockCrate(const void *data, uint32_t size);

    void BuildMatchmakingHello(CMsgGCCStrike15_v2_MatchmakingGC2ClientHello &message);
    void BuildClientWelcome(CMsgClientWelcome &message, const CMsgCStrike15Welcome &csWelcome,
                            const CMsgGCCStrike15_v2_MatchmakingGC2ClientHello &matchmakingHello);
    void SendRankUpdate();

    uint32_t AccountId() const { return m_steamId & 0xffffffff; }

    const uint64_t m_steamId;
    std::queue<OutgoingMessage> m_outgoingMessages;
    NetworkingClient m_networking{ this };

    GCConfig m_config;
    Inventory m_inventory;
};
