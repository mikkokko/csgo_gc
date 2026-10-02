#pragma once

#include "gc_const_csgo.h"
#include "inventory_editor.h"
#include "item.h"
#include "item_schema.h"
#include "random.h"

class InventoryModify;

struct SingleObject
{
    CMsgSOSingleObject proto;
    bool sendToGameServer;
};

// this should be revisited later... these semantics
// are not exactly correct, but it'll do for now
struct InventoryChangeMessages
{
    // full shared object cache update for clients
    CMsgSOMultipleObjects updatedClient;

    // janky... same as above but only contains equipped items
    CMsgSOMultipleObjects updatedGameServer;

    // client, or both client and game server
    std::vector<SingleObject> created;
    std::vector<SingleObject> destroyed;

    // client only
    CMsgGCItemCustomizationNotification notification;
};

using ItemMap = std::unordered_map<uint64_t, Item>;

class Inventory
{
public:
    Inventory(uint64_t steamId);
    ~Inventory();

    bool Update(InventoryChangeMessages &changeMessages);

    void BuildCacheSubscription(CMsgSOCacheSubscribed &message, int level, bool server);

    InventoryChangeMessages EquipItem(uint64_t itemId, uint32_t classId, uint32_t slotId);
    InventoryChangeMessages RemoveItem(uint64_t itemId);
    InventoryChangeMessages UseItem(uint64_t itemId);
    InventoryChangeMessages UnlockCrate(uint64_t crateId, uint64_t keyId);
    InventoryChangeMessages ApplySticker(const CMsgApplySticker &message);
    InventoryChangeMessages ScrapeSticker(const CMsgApplySticker &message);
    InventoryChangeMessages IncrementKillCountAttribute(uint64_t itemId, uint32_t amount);
    InventoryChangeMessages NameItem(uint64_t nameTagId, uint64_t itemId, std::string_view name);
    InventoryChangeMessages NameBaseItem(uint64_t nameTagId, uint32_t defIndex, std::string_view name);
    InventoryChangeMessages RemoveItemName(uint64_t itemId);
    InventoryChangeMessages CasketItemAdd(uint64_t casketId, uint64_t itemId);
    InventoryChangeMessages CasketItemExtract(uint64_t casketId, uint64_t itemId);

    InventoryChangeMessages SetItemPositions(const CMsgSetItemPositions &message, std::vector<CMsgItemAcknowledged> &acknowledgements);
    InventoryChangeMessages PurchaseItems(const std::vector<uint32_t> &defIndexes, std::vector<uint64_t> &itemIds);

    // called by InventoryModify when it goes out of scope
    void FlushChanges(const InventoryModify &modify);

private:
    uint32_t AccountId() const;

    InventoryChangeMessages BuildChangeMessages(
        const InventoryModify &modify,
        std::optional<EGCItemCustomizationNotification> customizationType = std::nullopt,
        std::initializer_list<uint64_t> customizationItemIds = {});

    // allocates an unique high item id
    // pass zero as highItemId if you don't want a specific one
    uint32_t GetHighItemId(uint32_t highItemId);

    // used to create items from inventory.txt
    void CreateItem(uint32_t highId, const KeyValue &kv);

    // bruh...
    Item &CreateItem(InventoryModify &modify, const ItemDesc &desc);

    // find an existing item by id, returns nullptr if not found
    Item *FindItem(uint64_t itemId);

    void ReadFromFile();
    void WriteToFile() const;

    bool EquipItem(InventoryModify &modify, uint64_t itemId, uint32_t classId, uint32_t slotId);
    bool UnequipItem(InventoryModify &modify, uint64_t itemId);
    void UnequipItem(InventoryModify &modify, uint32_t classId, uint32_t slotId);

    bool DestroyItemById(InventoryModify &modify, uint64_t itemId);
    void DestroyItem(InventoryModify &modify, Item *item);

    // helpers for serializing items to CMsgSOMultipleObjects and CMsgSOSingleObject
    void AddToMultipleObjects(CMsgSOMultipleObjects &message, SOTypeId type, const google::protobuf::MessageLite &object);
    void ToSingleObject(CMsgSOSingleObject &message, SOTypeId type, const google::protobuf::MessageLite &object);

    void SendFullInventoryToEditor();
    void SendChangesToEditor(const InventoryModify &modify);

    const uint64_t m_steamId;
    ItemSchema m_itemSchema;
    Random m_random;
    uint32_t m_lastHighItemId{};
    ItemMap m_items;
    std::vector<CSOEconDefaultEquippedDefinitionInstanceClient> m_defaultEquips;

    InventoryEditor m_editor;

    // stupid hack since we don't want to show casketed items in the editor
    std::unordered_set<uint32_t> m_editorVisibleItems;
};
