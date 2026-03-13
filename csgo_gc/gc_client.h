#pragma once

#include "config.h"
#include "gc_shared.h"
#include "inventory.h"

class ClientGC final : public SharedGC
{
public:
    ClientGC(uint64_t steamId);
    ~ClientGC();

private:
    void HandleEvent(GCEvent type, uint64_t id, const std::vector<uint8_t> &buffer) override;

    // event handlers
    void HandleMessage(uint32_t type, const void *data, uint32_t size);
    void HandleNetMessage(const void *data, uint32_t size);
    void HandleSOCacheRequest();

    // send to the local game and the game server we're connected to (if we're connected)
    void SendMessageToGame(bool sendToGameServer, uint32_t type,
        const google::protobuf::MessageLite &message, uint64_t jobId = JobIdInvalid);

    void OnClientHello(GCMessageRead &messageRead);
    void AdjustItemEquippedState(GCMessageRead &messageRead);
    void ClientPlayerDecalSign(GCMessageRead &messageRead);
    void UseItemRequest(GCMessageRead &messageRead);
    void ClientRequestJoinServerData(GCMessageRead &messageRead);
    void SetItemPositions(GCMessageRead &messageRead);
    void IncrementKillCountAttribute(GCMessageRead &messageRead);
    void ApplySticker(GCMessageRead &messageRead);
    void StoreGetUserData(GCMessageRead &messageRead);
    void StorePurchaseInit(GCMessageRead &messageRead);
    void StorePurchaseFinalize(GCMessageRead &messageRead);

    void DeleteItem(GCMessageRead &messageRead);
    void UnlockCrate(GCMessageRead &messageRead);
    void NameItem(GCMessageRead &messageRead);
    void NameBaseItem(GCMessageRead &messageRead);
    void RemoveItemName(GCMessageRead &messageRead);

    void BuildMatchmakingHello(CMsgGCCStrike15_v2_MatchmakingGC2ClientHello &message);
    void BuildClientWelcome(CMsgClientWelcome &message, const CMsgCStrike15Welcome &csWelcome,
        const CMsgGCCStrike15_v2_MatchmakingGC2ClientHello &matchmakingHello);
    void SendRankUpdate();

    uint32_t AccountId() const { return m_steamId & 0xffffffff; }

    const uint64_t m_steamId;

    Inventory m_inventory;

    // microtransactions, we only have one going at a time
    uint64_t m_transactionId{};
    std::vector<uint64_t> m_transactionItemIds;
};
