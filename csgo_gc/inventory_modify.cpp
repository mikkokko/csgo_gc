#include "stdafx.h"
#include "inventory_modify.h"

void InventoryModify::MarkItemCreated(uint32_t highId, bool gameServerDirty)
{
    auto existing = m_itemChanges.find(highId);
    if (existing != m_itemChanges.end())
    {
        assert(false);
        return;
    }

    m_itemChanges.try_emplace(highId, ItemChangeType::Created, gameServerDirty);
}

void InventoryModify::MarkItemDestroyed(uint32_t highId, bool gameServerDirty)
{
    auto existing = m_itemChanges.find(highId);
    if (existing == m_itemChanges.end())
    {
        m_itemChanges.try_emplace(highId, ItemChangeType::Destroyed, gameServerDirty);
        return;
    }

    switch (existing->second.type)
    {
    case ItemChangeType::Created:
        m_itemChanges.erase(existing);
        return;

    case ItemChangeType::Updated:
        existing->second.type = ItemChangeType::Destroyed;
        existing->second.gameServerDirty |= gameServerDirty;
        return;

    default:
        assert(false);
        break;
    }
}

void InventoryModify::MarkDefaultEquipChanged(const DefaultEquip &defaultEquip)
{
    for (DefaultEquip &existing : m_defaultEquipChanges)
    {
        if (existing.classId == defaultEquip.classId
            && existing.slotId == defaultEquip.slotId)
        {
            existing = defaultEquip;
            return;
        }
    }

    m_defaultEquipChanges.push_back(defaultEquip);
}

void InventoryModify::UpdateFromDesc(Item &item, ItemDesc &&desc)
{
    // FIXME: decide what to do with the equips, currently we don't apply then
    std::vector<ItemEquip> temp = std::move(item.m_desc.equips);
    item.m_desc = std::move(desc);
    item.m_desc.equips = std::move(temp);

    MarkUpdatedInternal(item.m_highId, item.HasEquips());
}

// for setting item positions...
void InventoryModify::SetItemInventory(Item &item, uint32_t inventory)
{
    if (item.m_desc.inventory != inventory)
    {
        item.m_desc.inventory = inventory;
        MarkUpdatedInternal(item.m_highId, item.HasEquips());
    }
}

void InventoryModify::SetItemAttribute(Item &item, AttributeDefIndex defIndex, ItemAttributeValue value)
{
    for (ItemAttribute &attribute : item.m_desc.attributes)
    {
        if (attribute.DefIndex() == defIndex)
        {
            if (attribute.Value() != value)
            {
                attribute.Value() = std::move(value);
                MarkUpdatedInternal(item.m_highId, item.HasEquips());
            }

            return;
        }
    }

    item.m_desc.attributes.emplace_back(defIndex, std::move(value));
    MarkUpdatedInternal(item.m_highId, item.HasEquips());
}

AttributeIncrement InventoryModify::IncrementItemAttribute(Item &item, AttributeDefIndex defIndex, int amount, uint32_t max)
{
    for (ItemAttribute &attribute : item.m_desc.attributes)
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
            MarkUpdatedInternal(item.m_highId, item.HasEquips());
            return AttributeIncrement::Ok;
        }
    }

    return AttributeIncrement::NoAttribute;
}

AttributeIncrement InventoryModify::IncrementItemAttribute(Item &item, AttributeDefIndex defIndex, float amount, float min, float max)
{
    for (ItemAttribute &attribute : item.m_desc.attributes)
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
            MarkUpdatedInternal(item.m_highId, item.HasEquips());
            return AttributeIncrement::Ok;
        }
    }

    return AttributeIncrement::NoAttribute;
}

void InventoryModify::RemoveItemAttributes(Item &item, std::initializer_list<AttributeDefIndex> types)
{
    auto predicate = [types](const ItemAttribute &attribute)
    {
        return std::find(types.begin(), types.end(), attribute.DefIndex()) != types.end();
    };

    if (std::erase_if(item.m_desc.attributes, predicate) > 0)
    {
        MarkUpdatedInternal(item.m_highId, item.HasEquips());
    }
}

void InventoryModify::AddItemEquip(Item &item, uint32_t classId, uint32_t slotId)
{
    item.m_desc.equips.emplace_back(classId, slotId);
    MarkUpdatedInternal(item.m_highId, true);
}

void InventoryModify::RemoveItemEquips(Item &item)
{
    if (!item.m_desc.equips.empty())
    {
        item.m_desc.equips.clear();
        MarkUpdatedInternal(item.m_highId, true);
    }
}

void InventoryModify::RemoveItemEquip(Item &item, uint32_t classId, uint32_t slotId)
{
    auto predicate = [classId, slotId](const ItemEquip &equip)
    {
        return equip.Class() == classId && equip.Slot() == slotId;
    };

    if (std::erase_if(item.m_desc.equips, predicate) > 0)
    {
        MarkUpdatedInternal(item.m_highId, true);
    }
}

void InventoryModify::MarkUpdatedInternal(uint32_t highId, bool gameServerDirty)
{
    auto existing = m_itemChanges.find(highId);
    if (existing == m_itemChanges.end())
    {
        m_itemChanges.try_emplace(highId, ItemChangeType::Updated, gameServerDirty);
        return;
    }

    switch (existing->second.type)
    {
    case ItemChangeType::Created:
    case ItemChangeType::Updated:
        // valid, update whether equips were touched
        existing->second.gameServerDirty |= gameServerDirty;
        break;

    default:
        // shouldn't happen
        assert(false);
        break;
    }
}
