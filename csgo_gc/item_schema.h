#pragma once

class KeyValue;

enum class AttributeType
{
    Float,
    Uint32,
    String
};

class AttributeInfo
{
public:
    AttributeInfo(const KeyValue &key);

    AttributeType m_type;
};

class ItemInfo
{
public:
    ItemInfo(uint32_t defIndex);

    uint32_t m_defIndex;
    std::string m_name;
    uint32_t m_rarity;
    uint32_t m_quality;
    uint32_t m_supplyCrateSeries; // cases only
    uint32_t m_tournamentEventId; // souvenirs only
};

class PaintKitInfo
{
public:
    PaintKitInfo(const KeyValue &key);

    uint32_t m_defIndex;
    uint32_t m_rarity;
    float m_minFloat;
    float m_maxFloat;
};


class StickerKitInfo
{
public:
    StickerKitInfo(const KeyValue &key);

    uint32_t m_defIndex;
    uint32_t m_rarity;
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
    const ItemInfo *itemInfo{};
    LootListItemType type{ LootListItemNoAttribute };

    // these could be sticked into a variant to save a grand total of few bytes
    const PaintKitInfo *paintKitInfo{};
    const StickerKitInfo *stickerKitInfo{};
    const MusicDefinitionInfo *musicDefinitionInfo{};

    // might differ from those specified in itemInfo
    // (based on paint kits, stattrak etc.)
    uint32_t rarity{};
    uint32_t quality{};
};

struct LootList
{
    // we either have items or sublists, never both
    std::vector<LootListItem> items;
    std::vector<const LootList *> subLists;
    bool willProduceStatTrak{};
    bool isUnusual{};
};

// mikkotodo unfuck
enum class GenerateStatTrak
{
    No,
    Yes,
    Maybe
};

class ItemSchema
{
public:
    ItemSchema();

    float AttributeFloat(const CSOEconItemAttribute *attribute) const;
    uint32_t AttributeUint32(const CSOEconItemAttribute *attribute) const;
    std::string AttributeString(const CSOEconItemAttribute *attribute) const;

    bool SetAttributeFloat(CSOEconItemAttribute *attribute, float value) const;
    bool SetAttributeUint32(CSOEconItemAttribute *attribute, uint32_t value) const;
    bool SetAttributeString(CSOEconItemAttribute *attribute, std::string_view value) const;

    // case opening
    bool SelectItemFromCrate(const CSOEconItem &crate, CSOEconItem &item);

public:
    // these could be parsed from the item schema but reduce code complexity by hardcoding them
    enum Rarity
    {
        RarityDefault = 0,
        RarityCommon = 1,
        RarityUncommon = 2,
        RarityRare = 3,
        RarityMythical = 4,
        RarityLegendary = 5,
        RarityAncient = 6,
        RarityImmortal = 7,

        RarityUnusual = 99
    };

    enum Quality
    {
        QualityNormal = 0,
        QualityGenuine = 1,
        QualityVintage = 2,
        QualityUnusual = 3,
        QualityUnique = 4,
        QualityCommunity = 5,
        QualityDeveloper = 6,
        QualitySelfmade = 7,
        QualityCustomized = 8,
        QualityStrange = 9,
        QualityCompleted = 10,
        QualityHaunted = 11,
        QualityTournament = 12
    };

    enum GraffitiTint
    {
        GraffitiTintMin = 1,
        GraffitiTintMax = 19
    };

    enum LoadoutSlot
    {
        LoadoutSlotGraffiti = 56
    };

    enum Item
    {
        ItemSpray = 1348,
        ItemSprayPaint = 1349,
        ItemPatch = 4609
    };

    enum Attribute
    {
        AttributeTexturePrefab = 6,
        AttributeTextureSeed = 7,
        AttributeTextureWear = 8,
        AttributeKillEater = 80,
        AttributeKillEaterScoreType = 81,

        AttributeCustomName = 111,

        // ugh
        AttributeStickerId0 = 113,
        AttributeStickerWear0 = 114,
        AttributeStickerScale0 = 115,
        AttributeStickerRotation0 = 116,
        AttributeStickerId1 = 117,
        AttributeStickerWear1 = 118,
        AttributeStickerScale1 = 119,
        AttributeStickerRotation1 = 120,
        AttributeStickerId2 = 121,
        AttributeStickerWear2 = 122,
        AttributeStickerScale2 = 123,
        AttributeStickerRotation2 = 124,
        AttributeStickerId3 = 125,
        AttributeStickerWear3 = 126,
        AttributeStickerScale3 = 127,
        AttributeStickerRotation3 = 128,
        AttributeStickerId4 = 129,
        AttributeStickerWear4 = 130,
        AttributeStickerScale4 = 131,
        AttributeStickerRotation4 = 132,
        AttributeStickerId5 = 133,
        AttributeStickerWear5 = 134,
        AttributeStickerScale5 = 135,
        AttributeStickerRotation5 = 136,

        AttributeMusicId = 166,
        AttributeQuestId = 168,

        AttributeSpraysRemaining = 232,
        AttributeSprayTintId = 233,
    };


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

    // case opening
    bool EconItemFromLootListItem(const LootListItem &lootListItem, CSOEconItem &item, GenerateStatTrak statTrak);

    std::unordered_map<uint32_t, ItemInfo> m_itemInfo;
    std::unordered_map<uint32_t, AttributeInfo> m_attributeInfo;

    std::unordered_map<std::string, StickerKitInfo> m_stickerKitInfo;
    std::unordered_map<std::string, PaintKitInfo> m_paintKitInfo;
    std::unordered_map<std::string, MusicDefinitionInfo> m_musicDefinitionInfo;
    std::unordered_map<std::string, LootList> m_lootLists;

    std::unordered_map<uint32_t, const LootList &> m_revolvingLootLists;
};
