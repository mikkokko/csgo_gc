#pragma once

#include "config.h"
#include "gc_shared.h"
#include "inventory.h"
#include "networking_client.h"

class ClientGC final : public SharedGC
{
public:
    ClientGC(uint64_t steamId);
    ~ClientGC();

    void HandleMessage(uint32_t type, const void *data, uint32_t size);

    void Update();

    // called from net code
    void SendSOCacheToGameSever();
    void HandleNetMessage(GCMessageRead &messageRead);

    // passed to net code
    void SetAuthTicket(uint32_t handle, const void *data, uint32_t size);
    void ClearAuthTicket(uint32_t handle);

private:
    // send to the local game and the game server we're connected to (if we're connected)
    void SendMessageToGame(bool sendToGameServer, uint32_t type, const google::protobuf::MessageLite &message);

    void OnClientHello(GCMessageRead &messageRead);
    void AdjustItemEquippedState(GCMessageRead &messageRead);
    void ClientPlayerDecalSign(GCMessageRead &messageRead);
    void UseItemRequest(GCMessageRead &messageRead);
    void ClientRequestJoinServerData(GCMessageRead &messageRead);
    void SetItemPositions(GCMessageRead &messageRead);
    void IncrementKillCountAttribute(GCMessageRead &messageRead);
    void ApplySticker(GCMessageRead &messageRead);

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
    NetworkingClient m_networking{ this };

    GCConfig m_config;
    Inventory m_inventory;
};
