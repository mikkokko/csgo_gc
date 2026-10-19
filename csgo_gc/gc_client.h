#pragma once

#include "gc_const.h"
#include "gc_shared.h"
#include "inventory.h"

class GCMessageRead;

class CMsgClientWelcome;
class CMsgCStrike15Welcome;
class CMsgGCCStrike15_v2_MatchmakingGC2ClientHello;

struct AuthTicket
{
    uint64_t steamId{}; // gameserver
    std::vector<uint8_t> buffer;
};

class ClientGC final : public SharedGC
{
public:
    ClientGC(uint64_t steamId);
    ~ClientGC();

private:
    void HandleEvent(GCEvent type, uint64_t id, const std::vector<uint8_t> &buffer) override;

    // event handlers
    void HandleMessage(uint32_t type, const void *data, uint32_t size);
    void HandleNetMessage(uint64_t steamId, const void *data, uint32_t size);
    void SetAuthTicket(uint32_t handle, const std::vector<uint8_t> &buffer);
    void ClearAuthTicket(uint32_t handle);
    void HandleConnect(uint64_t steamId, GCMessageRead &message);
    void HandleSOCacheRequest();
    void InventoryUpdate();

    // send to the local game and the game server we're connected to (if we're connected)
    void SendMessageToGame(bool sendToGameServer, uint32_t type,
        const google::protobuf::MessageLite &message, uint64_t jobId = JobIdInvalid);

    void SendInventoryChangeMessages(const InventoryChangeMessages &messages);
    void SendMessageToServer(std::vector<uint8_t> &&buffer);

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
    void OnGetEventFavorites(GCMessageRead &messageRead);
    void OnPartyRegister(GCMessageRead &messageRead);

    void DeleteItem(GCMessageRead &messageRead);
    void UnlockCrate(GCMessageRead &messageRead);
    void NameItem(GCMessageRead &messageRead);
    void NameBaseItem(GCMessageRead &messageRead);
    void RemoveItemName(GCMessageRead &messageRead);

    void ProcessCasketItemLoadContents(GCMessageRead &messageRead);
    void ProcessCasketItemAdd(GCMessageRead &messageRead);
    void ProcessCasketItemExtract(GCMessageRead &messageRead);

    void BuildMatchmakingHello(CMsgGCCStrike15_v2_MatchmakingGC2ClientHello &message);
    void BuildClientWelcome(CMsgClientWelcome &message, const CMsgCStrike15Welcome &csWelcome,
        const CMsgGCCStrike15_v2_MatchmakingGC2ClientHello &matchmakingHello);
    void SendRankUpdate();

    uint32_t AccountId() const { return m_steamId & 0xffffffff; }

    const uint64_t m_steamId;

    Inventory m_inventory;

    uint64_t m_serverSteamId{};
    std::unordered_map<uint32_t, AuthTicket> m_tickets;

    // microtransactions, we only have one going at a time
    uint64_t m_transactionId{};
    std::vector<uint64_t> m_transactionItemIds;
};
