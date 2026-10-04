#pragma once

#include "gc_const_csgo.h"
#include "item_schema_const.h"

class KeyValue;
class Random;

struct ItemDesc;

enum class AttributeType
{
    Float,
    Uint32,
    String
};

class AttributeInfo
{
public:
    explicit AttributeInfo(const KeyValue &key);

    AttributeType m_type;
};

class ItemInfo
{
public:
    explicit ItemInfo(ItemDefIndex defIndex);

    ItemDefIndex m_defIndex;
    std::string m_name;
    Rarity m_rarity;
    Quality m_quality;
    uint32_t m_level;
    uint32_t m_supplyCrateSeries; // cases only

    // kludge for coupons so we can buy stuff from the store
    bool m_isCoupon;
    std::string m_lootListName;
    bool m_willProduceStatTrak;
};

class PaintKitInfo
{
public:
    explicit PaintKitInfo(const KeyValue &key);

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

    const ItemInfo *itemInfo{};
    LootListItemType type{ LootListItemNoAttribute };

    // these could be sticked into a variant to save a grand total of few bytes
    const PaintKitInfo *paintKitInfo{};
    const StickerKitInfo *stickerKitInfo{};
    const MusicDefinitionInfo *musicDefinitionInfo{};

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
};

class ItemSchema
{
public:
    ItemSchema();

    AttributeType GetAttributeType(AttributeDefIndex defIndex) const;

    // for case opening
    const LootList *GetCrateLootList(ItemDefIndex crateDefIndex) const;

    // for case opening FIXME: do we want to keep this here???
    bool ItemDescForLootListItem(Random &random,
        const LootListItem &lootListItem,
        bool statTrak,
        ItemOrigin origin,
        UnacknowledgedType unacknowledgedType,
        ItemDesc &desc) const;

    // item creation
    bool GetItemDesc(ItemDefIndex defIndex, ItemOrigin origin, UnacknowledgedType unacknowledgedType, ItemDesc &desc) const;

private:
    void ParseItems(const KeyValue *itemsKey, const KeyValue *prefabsKey);
    void ParseItemRecursive(ItemInfo &info, const KeyValue &itemKey, const KeyValue *prefabsKey);
    void ParseAttributes(const KeyValue *attributesKey);
    void ParseStickerKits(const KeyValue *stickerKitsKey);
    void ParsePaintKits(const KeyValue *paintKitsKey);
    void ParsePaintKitRarities(const KeyValue *raritiesKey);
    void ParseMusicDefinitions(const KeyValue *musicDefinitionsKey);
    void ParseLootLists(const KeyValue *lootListsKey, bool unusual);
    void ParseRevolvingLootLists(const KeyValue *revolvingLootListsKey);

    bool ParseLootListItem(LootListItem &item, std::string_view name);

    // internal slop
    ItemInfo *ItemInfoByName(std::string_view name);
    StickerKitInfo *StickerKitInfoByName(std::string_view name);
    PaintKitInfo *PaintKitInfoByName(std::string_view name);
    MusicDefinitionInfo *MusicDefinitionInfoByName(std::string_view name);

    std::unordered_map<ItemDefIndex, ItemInfo> m_itemInfo;
    std::unordered_map<AttributeDefIndex, AttributeInfo> m_attributeInfo;

    std::unordered_map<std::string, StickerKitInfo> m_stickerKitInfo;
    std::unordered_map<std::string, PaintKitInfo> m_paintKitInfo;
    std::unordered_map<std::string, MusicDefinitionInfo> m_musicDefinitionInfo;
    std::unordered_map<std::string, LootList> m_lootLists;

    std::unordered_map<uint32_t, const LootList &> m_revolvingLootLists;
};
