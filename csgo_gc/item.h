#pragma once

#include "item_schema_const.h"
#include "gc_const_csgo.h"

class ItemSchema;
class KeyValue;
class KeyValueWrite;

class CSOEconItem;
class CEconItemPreviewDataBlock;

// not item related per se, but where else would this go...
struct DefaultEquip
{
    uint32_t defIndex;
    uint32_t classId;
    uint32_t slotId;
};

using ItemAttributeValue = std::variant<float, uint32_t, std::string>;

class ItemAttribute
{
public:
    ItemAttribute(AttributeDefIndex defIndex, ItemAttributeValue value)
        : m_defIndex{ defIndex }
        , m_value{ std::move(value) }
    {
    }

    AttributeDefIndex DefIndex() const { return m_defIndex; }

    const ItemAttributeValue &Value() const { return m_value; }
    ItemAttributeValue &Value() { return m_value; }

    template<typename T>
    T ValueAs() const
    {
        const T *value = std::get_if<T>(&m_value);
        if (!value)
        {
            // almost certainly a programmer error
            assert(false);
            return T{};
        }

        return *value;
    }

private:
    AttributeDefIndex m_defIndex;
    ItemAttributeValue m_value;
};

class ItemEquip
{
public:
    template<typename... ValueArgs>
    ItemEquip(uint32_t equipClass, uint32_t equipSlot)
        : m_class{ equipClass }
        , m_slot{ equipSlot }
    {
    }

    uint32_t Class() const { return m_class; }
    uint32_t Slot() const { return m_slot; }

private:
    uint32_t m_class;
    uint32_t m_slot;
};

struct ItemDesc
{
    uint32_t inventory;
    ItemDefIndex defIndex;
    uint32_t level;
    Quality quality;
    uint32_t flags;
    uint32_t origin;
    bool inUse;
    Rarity rarity;
    std::vector<ItemAttribute> attributes;
    std::vector<ItemEquip> equips;
};

class Item
{
public:
    // used on init
    Item(uint32_t lowId, uint32_t accountId, const KeyValue &kv, const ItemSchema &itemSchema);

    // used by InventoryModify to create items
    Item(uint32_t lowId, ItemDesc &&desc);

    // stupid
    uint64_t FullIdFor(uint32_t accountId) const
    {
        return ComposeItemId(accountId, m_lowId);
    }

    // serialization...
    void ToKeyValue(KeyValueWrite &kv) const;
    void ToCSOEconItem(CSOEconItem &item, uint32_t accountId, const ItemSchema &itemSchema, bool oldProtocol) const;
    void ToEconItemPreviewDataBlock(CEconItemPreviewDataBlock &block, uint32_t accountId) const;

    uint32_t LowId() const { return m_lowId; }
    const ItemDesc &GetDesc() const { return m_desc; }

    // FIXME: cull some of the stuff below in favour of using GetDesc???
    ItemDefIndex DefIndex() const { return m_desc.defIndex; }
    Rarity GetRarity() const { return m_desc.rarity; }

    // could return a span later if needed
    bool HasEquips() const { return !m_desc.equips.empty(); }

    bool HasAttribute(AttributeDefIndex defIndex) const
    {
        for (const ItemAttribute &attribute : m_desc.attributes)
        {
            if (attribute.DefIndex() == defIndex)
            {
                return true;
            }
        }

        return false;
    }

    template<typename T>
    T GetAttributeValue(AttributeDefIndex defIndex) const
    {
        for (const ItemAttribute &attribute : m_desc.attributes)
        {
            if (attribute.DefIndex() == defIndex)
            {

                return attribute.ValueAs<T>();
            }
        }

        return T{};
    }

private:
    friend class InventoryModify;

    const uint32_t m_lowId;
    ItemDesc m_desc{};
};
