#include "stdafx.h"
#include "inventory_modify.h"
#include "inventory.h"

InventoryModify::InventoryModify(const Inventory &inventory)
    : m_inventory{ inventory }
{
}

InventoryModify::~InventoryModify()
{
    if (!m_itemChanges.empty() || !m_defaultEquipChanges.empty())
    {
        m_inventory.WriteToFile();
    }
}

void InventoryModify::MarkItemCreated(uint32_t highId)
{
    auto existing = m_itemChanges.find(highId);
    if (existing != m_itemChanges.end())
    {
        assert(false);
        return;
    }

    m_itemChanges.try_emplace(highId, ItemChangeType::Created);
}

void InventoryModify::MarkItemDestroyed(uint32_t highId)
{
    auto existing = m_itemChanges.find(highId);
    if (existing == m_itemChanges.end())
    {
        m_itemChanges.try_emplace(highId, ItemChangeType::Destroyed);
        return;
    }

    switch (existing->second)
    {
    case ItemChangeType::Created:
        m_itemChanges.erase(existing);
        return;

    case ItemChangeType::Updated:
        existing->second = ItemChangeType::Destroyed;
        return;

    default:
        assert(false);
        break;
    }
}

void InventoryModify::MarkDefaultEquipChanged(const CSOEconDefaultEquippedDefinitionInstanceClient &defaultEquip)
{
    for (CSOEconDefaultEquippedDefinitionInstanceClient &existing : m_defaultEquipChanges)
    {
        if (existing.class_id() == defaultEquip.class_id()
            && existing.slot_id() == defaultEquip.slot_id())
        {
            existing = defaultEquip;
            return;
        }
    }

    m_defaultEquipChanges.push_back(defaultEquip);
}

// for setting item positions...
void InventoryModify::SetItemInventory(Item &item, uint32_t inventory)
{
    if (item.m_inventory != inventory)
    {
        item.m_inventory = inventory;
        MarkUpdatedInternal(item.m_highId);
    }
}

// always just adds a new attribute, one with the same def index can already exist
void InventoryModify::AddItemAttribute(Item &item, AttributeDefIndex defIndex, ItemAttributeValue value)
{
    item.m_attributes.emplace_back(defIndex, value);
    MarkUpdatedInternal(item.m_highId);
}

// set attribute value if it exists, using this only makes sense if there's only 1 attribute with the def index
bool InventoryModify::SetItemAttribute(Item &item, AttributeDefIndex defIndex, ItemAttributeValue value, bool addIfMissing)
{
    for (ItemAttribute &attribute : item.m_attributes)
    {
        if (attribute.DefIndex() == defIndex)
        {
            if (attribute.Value() != value)
            {
                attribute.Value() = value;
                MarkUpdatedInternal(item.m_highId);
            }

            return true;
        }
    }

    if (addIfMissing)
    {
        item.m_attributes.emplace_back(defIndex, value);
        MarkUpdatedInternal(item.m_highId);
        return true;
    }

    return false;
}

AttributeIncrement InventoryModify::IncrementItemAttribute(Item &item, AttributeDefIndex defIndex, int amount, uint32_t max)
{
    for (ItemAttribute &attribute : item.m_attributes)
    {
        if (attribute.DefIndex() == defIndex)
        {
            uint32_t *value = std::get_if<uint32_t>(&attribute.Value());
            if (!value)
            {
                assert(false);
                return AttributeIncrement::NoAttribute;
            }

            int64_t newValue = static_cast<int64_t>(*value) + amount;
            if (newValue < 0 || newValue > max)
            {
                return AttributeIncrement::OutOfRange;
            }

            *value = static_cast<uint32_t>(newValue);
            MarkUpdatedInternal(item.m_highId);
            return AttributeIncrement::Ok;
        }
    }

    return AttributeIncrement::NoAttribute;
}

AttributeIncrement InventoryModify::IncrementItemAttribute(Item &item, AttributeDefIndex defIndex, float amount, float min, float max)
{
    for (ItemAttribute &attribute : item.m_attributes)
    {
        if (attribute.DefIndex() == defIndex)
        {
            float *value = std::get_if<float>(&attribute.Value());
            if (!value)
            {
                assert(false);
                return AttributeIncrement::NoAttribute;
            }

            float newValue = *value + amount;
            if (newValue < min || newValue > max)
            {
                return AttributeIncrement::OutOfRange;
            }

            *value = newValue;
            MarkUpdatedInternal(item.m_highId);
            return AttributeIncrement::Ok;
        }
    }

    return AttributeIncrement::NoAttribute;
}

void InventoryModify::RemoveItemAttributes(Item &item, std::initializer_list<AttributeDefIndex> types)
{
    bool modified = false;

    for (auto it = item.m_attributes.begin(); it != item.m_attributes.end();)
    {
        bool remove = false;

        for (AttributeDefIndex type : types)
        {
            if (it->DefIndex() == type)
            {
                remove = true;
                break;
            }
        }

        if (remove)
        {
            it = item.m_attributes.erase(it);
            modified = true;
        }
        else
        {
            it++;
        }
    }

    if (modified)
    {
        MarkUpdatedInternal(item.m_highId);
    }
}

void InventoryModify::AddItemEquip(Item &item, uint32_t classId, uint32_t slotId)
{
    item.m_equips.emplace_back(classId, slotId);
    MarkUpdatedInternal(item.m_highId);
}

void InventoryModify::RemoveItemEquips(Item &item)
{
    if (!item.m_equips.empty())
    {
        item.m_equips.clear();
        MarkUpdatedInternal(item.m_highId);
    }
}

bool InventoryModify::RemoveItemEquip(Item &item, uint32_t classId, uint32_t slotId)
{
    bool modified = false;

    for (auto it = item.m_equips.begin(); it != item.m_equips.end();)
    {
        if (it->Class() == classId && it->Slot() == slotId)
        {
            it = item.m_equips.erase(it);
            modified = true;
        }
        else
        {
            it++;
        }
    }

    if (modified)
    {
        MarkUpdatedInternal(item.m_highId);
    }

    return modified;
}

void InventoryModify::MarkUpdatedInternal(uint32_t highId)
{
    auto existing = m_itemChanges.find(highId);
    if (existing == m_itemChanges.end())
    {
        m_itemChanges.try_emplace(highId, ItemChangeType::Updated);
        return;
    }

    switch (existing->second)
    {
    case ItemChangeType::Created:
    case ItemChangeType::Updated:
        // ok
        break;

    default:
        assert(false);
        break;
    }
}
