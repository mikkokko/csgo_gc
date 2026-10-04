#pragma once

#include "item.h"

class Inventory;

enum class ItemChangeType
{
    Created,
    Updated,
    Destroyed
};

struct ItemChange
{
    ItemChangeType type;
    bool gameServerDirty;
};

enum class AttributeIncrement
{
    Ok,
    NoAttribute,
    OutOfRange
};

// record changes made to the inventory so we can build the correct
// messages for the game to update its shared object cache
class InventoryModify
{
public:
    InventoryModify(Inventory &inventory);
    ~InventoryModify();

    InventoryModify(const InventoryModify &) = delete;
    InventoryModify(InventoryModify &&) = delete;

    InventoryModify &operator=(const InventoryModify &) = delete;
    InventoryModify &operator=(InventoryModify &&) = delete;

    // manual bookkeeping
    void MarkItemCreated(uint32_t highId, bool gameServerDirty);
    void MarkItemDestroyed(uint32_t highId, bool gameServerDirty);
    void MarkDefaultEquipChanged(const DefaultEquip &defaultEquip);

    // full update from the editor, except for equips as those need manual handling
    void UpdateFromDesc(Item &item, const ItemDesc &desc);

    // for setting item positions...
    void SetItemInventory(Item &item, uint32_t inventory);

    // sets the value for the first attribute with a matching def index, adds the attribute if it doesn't exist
    void SetItemAttribute(Item &item, AttributeDefIndex defIndex, ItemAttributeValue value);

    AttributeIncrement IncrementItemAttribute(Item &item, AttributeDefIndex defIndex, int amount, uint32_t max = std::numeric_limits<uint32_t>::max());
    AttributeIncrement IncrementItemAttribute(Item &item, AttributeDefIndex defIndex, float amount, float min, float max);
    void RemoveItemAttributes(Item &item, std::initializer_list<AttributeDefIndex> types);

    void AddItemEquip(Item &item, uint32_t classId, uint32_t slotId);
    void RemoveItemEquips(Item &item);
    bool RemoveItemEquip(Item &item, uint32_t classId, uint32_t slotId);

    std::unordered_map<uint32_t, ItemChange> m_itemChanges;
    std::vector<DefaultEquip> m_defaultEquipChanges;

private:
    void MarkUpdatedInternal(uint32_t highId, bool gameServerDirty);

    Inventory &m_inventory;
};
