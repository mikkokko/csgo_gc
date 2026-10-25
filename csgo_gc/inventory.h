#pragma once

#include "gc_const_csgo.h"
#include "inventory_editor.h"
#include "item.h"
#include "item_schema.h"

class CMsgApplySticker;
class CMsgSetItemPositions;
class CMsgItemAcknowledged;

class CMsgSOCacheSubscribed;
class CMsgSOMultipleObjects;
class CMsgSOSingleObject;

enum EGCItemCustomizationNotification : int;

class InventoryModify;
struct InventoryChangeMessages;

class Inventory
{
public:
    Inventory(uint64_t steamId);
    ~Inventory();

    // called on client welcome
    void SetOldProtocol(bool oldProtocol) { m_oldProtocol = oldProtocol; }

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

private:
    uint32_t AccountId() const;

    // increment version, build messages for the game, send changes to editor and write to file
    InventoryChangeMessages FinishChanges(
        InventoryModify &modify,
        std::optional<EGCItemCustomizationNotification> customizationType = std::nullopt,
        std::initializer_list<uint64_t> customizationItemIds = {});

    // allocates an unique low item id
    // pass zero as lowItemId if you don't want a specific one
    uint32_t GetLowItemId(uint32_t lowItemId);

    // used to create items from inventory.txt
    void CreateItem(uint32_t lowId, const KeyValue &kv);

    // used to create items at runtime
    Item &CreateItem(InventoryModify &modify, ItemDesc &&desc);

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

    void RemoveCachedIcon(uint32_t lowId) const;

    const uint64_t m_steamId;
    ItemSchema m_itemSchema;
    bool m_oldProtocol{ false };
    uint32_t m_lastLowItemId{};
    uint64_t m_version{ 1 };
    std::unordered_map<uint64_t, Item> m_items;
    std::vector<DefaultEquip> m_defaultEquips;

    InventoryEditor m_editor;

    // stupid hack since we don't want to show casketed items in the editor
    std::unordered_set<uint32_t> m_editorVisibleItems;
};
