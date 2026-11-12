#pragma once

#include "gc_const_csgo.h"
#include "item_schema_const.h"
#include "item.h" // FIXME: avoid dragging this in...

class KeyValue;

struct ItemDesc;

enum class AttributeType
{
    Float,
    Uint32,
    String
};

struct ItemAttributeInfo
{
    AttributeDefIndex defIndex;
    ItemAttributeValue value;
};

class ItemInfo
{
public:
    explicit ItemInfo(ItemDefIndex defIndex);

    ItemDefIndex m_defIndex;
    std::string m_name;
    std::string m_imageInventory;
    Rarity m_rarity;
    Quality m_quality;
    uint32_t m_level;
    bool m_isCoupon;
    bool m_isSelfOpeningPurchase;
    uint32_t m_storageIcon; // rio2022 caskets only...
    std::string m_lootListName;
    bool m_willProduceStatTrak;

    std::vector<ItemAttributeInfo> m_attributes;
};

class PaintKitInfo
{
public:
    explicit PaintKitInfo(const KeyValue &key, float fallbackMinFloat, float fallbackMaxFloat);

    uint32_t m_defIndex;
    Rarity m_rarity;
    float m_minFloat;
    float m_maxFloat;
};

class StickerKitInfo
{
public:
    explicit StickerKitInfo(const KeyValue &key);

    uint32_t m_defIndex;
    Rarity m_rarity;
};

class MusicDefinitionInfo
{
public:
    MusicDefinitionInfo(const KeyValue &key);

    uint32_t m_defIndex;
};

enum LootListItemType
{
    LootListItemNoAttribute,
    LootListItemPaintable,
    LootListItemSticker,
    LootListItemSpray,
    LootListItemPatch,
    LootListItemMusicKit,
};

struct LootListItem
{
    // for case opening: returns RarityUnusual for items of unusual quality
    Rarity CaseRarity() const;

    ItemDefIndex itemDefIndex{};
    LootListItemType type{ LootListItemNoAttribute };

    // paint/sticker/music kit
    uint32_t kitDefIndex{};

    // paint kit wear
    float minFloat{};
    float maxFloat{};

    // might differ from those specified in itemInfo
    // (based on paint kits, stattrak etc.)
    Rarity rarity{};
    Quality quality{};
};

struct LootList
{
    // we either have items or sublists, never both
    std::vector<LootListItem> items;
    std::vector<const LootList *> subLists;
    bool willProduceStatTrak{};
    bool isUnusual{};
    bool allEntriesAsAdditionalDrops{};
};

struct ItemSet
{
    // might as well parse to full blown loot list items
    std::vector<LootListItem> items;
};

inline uint64_t ItemToSetKey(ItemDefIndex item, uint32_t kit)
{
    return (static_cast<uint64_t>(item) << 32) | kit;
}

class ItemSchema
{
public:
    ItemSchema();

    AttributeType GetAttributeType(AttributeDefIndex defIndex) const;

    // for case opening
    const LootList *GetCrateLootList(ItemDefIndex crateDefIndex) const;
    uint32_t GetCrateSupplyCrateSeries(ItemDefIndex crateDefIndex) const;

    // for case opening FIXME: do we want to keep this here???
    bool ItemDescForLootListItem(
        const LootListItem &lootListItem,
        bool statTrak,
        ItemOrigin origin,
        UnacknowledgedType unacknowledgedType,
        ItemDesc &desc) const;

    const ItemSet *ItemSetFromItemAndKit(ItemDefIndex item, uint32_t kit) const;

    // FIXME: see about merging with ItemDescForLootListItem
    bool GetCraftOutput(const LootListItem &item, float wear, bool statTrak, ItemDesc &desc) const;

    // item creation
    bool GetItemDesc(ItemDefIndex defIndex, ItemOrigin origin, UnacknowledgedType unacknowledgedType, ItemDesc &desc) const;
    bool GetPurchaseItemDescs(ItemDefIndex defIndex, std::vector<ItemDesc> &items, std::vector<ItemDesc> &casketedItems) const;

private:
    void ParseItems(const KeyValue *itemsKey, const KeyValue *prefabsKey);
    void ParseItemRecursive(ItemInfo &info, const KeyValue &itemKey, const KeyValue *prefabsKey);
    void ParseItemAttribute(ItemInfo &itemInfo, const KeyValue &attributeKey);
    void ParseAttributes(const KeyValue *attributesKey);
    void ParseStickerKits(const KeyValue *stickerKitsKey);
    void ParsePaintKits(const KeyValue *paintKitsKey);
    void ParsePaintKitRarities(const KeyValue *raritiesKey);
    void ParseMusicDefinitions(const KeyValue *musicDefinitionsKey);
    void ParseLootLists(const KeyValue *lootListsKey);
    void ParseRevolvingLootLists(const KeyValue *revolvingLootListsKey);
    void ParseItemSets(const KeyValue *itemSetsKey);

    bool ParseLootListItem(LootListItem &item, std::string_view name, bool isUnusual);

    // internal slop
    ItemInfo *ItemInfoByName(std::string_view name);
    StickerKitInfo *StickerKitInfoByName(std::string_view name);
    PaintKitInfo *PaintKitInfoByName(std::string_view name);
    MusicDefinitionInfo *MusicDefinitionInfoByName(std::string_view name);

    std::unordered_map<ItemDefIndex, ItemInfo> m_itemInfo;
    std::unordered_map<AttributeDefIndex, AttributeType> m_attributeTypes;

    // these shouldn't be needed at runtime, need to try factoring out later
    StringMap<AttributeDefIndex> m_attributeDefIndexes;
    StringMap<StickerKitInfo> m_stickerKitInfo;
    StringMap<PaintKitInfo> m_paintKitInfo;
    StringMap<MusicDefinitionInfo> m_musicDefinitionInfo;

    StringMap<LootList> m_lootLists;

    std::unordered_map<uint32_t, const LootList &> m_revolvingLootLists;

    // garbage for arms deal contracts
    std::vector<ItemSet> m_itemSets;

    // look up what set item belongs to, paint kit and item def index combined into u64
    std::unordered_map<uint64_t, size_t> m_itemToSet;
};
