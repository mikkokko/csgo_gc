#pragma once

class KeyValue;

struct AttributeInfo
{
    bool storedAsInteger{};
};

struct ItemInfo
{
    uint32_t defIndex; // mikkotodo could remove
    std::string name;
    uint32_t rarity{ 1 }; // fallback to 1, which means common (mikkotodo ugly)
    uint32_t quality{ 4 }; // fallback to 4, which means unique (mikkotodo ugly)
    uint32_t supplyCrateSeries{}; // cases only
};

struct PaintKitInfo
{
    uint32_t defIndex; // mikkotodo could remove
    std::string name;
    uint32_t rarity{ 0 }; // fallback to 0, which means default (mikkotodo ugly)
    float minFloat;
    float maxFloat;
};

struct StickerKitInfo
{
    uint32_t defIndex; // mikkotodo could remove
    std::string name;
    uint32_t rarity;
};

struct MusicDefinitionInfo
{
    std::string name;
};

enum LootListItemType
{
    LootListItemSticker,
    LootListItemSpray,
    LootListItemPatch,
    LootListItemMusicKit,

    // fallback
    LootListItemPaintable,

    // even faller back (bruh)
    LootListItemNoAttribute
};

struct LootListItem
{
    const ItemInfo *itemInfo;
    LootListItemType type;

    // depends on the item type
    union Attribute
    {
        const PaintKitInfo *paintKitInfo;
        uint32_t stickerKitIndex;
        uint32_t musicDefinitionIndex;
    } attribute;

    uint32_t raritynew; // might differ from iteminfo's
    uint32_t quality; // might differ from iteminfo's (forced stattrak)
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

    bool AttributeStoredAsInteger(uint32_t defIndex) const;

    int AttributeValueInt(const CSOEconItemAttribute &attribute) const;
    float AttributeValueFloat(const CSOEconItemAttribute &attribute) const;

    void SetAttributeValueInt(CSOEconItemAttribute &attribute, int value) const;
    void SetAttributeValueFloat(CSOEconItemAttribute &attribute, float value) const;

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
    uint32_t MusicDefinitionIndexByName(std::string_view name) const;

    // case opening
    bool EconItemFromLootListItem(const LootListItem &lootListItem, CSOEconItem &item, GenerateStatTrak statTrak);
    const LootListItem &SelectItemFromLists(const std::vector<const LootList *> &lists);

    std::unordered_map<uint32_t, ItemInfo> m_itemInfo;
    std::unordered_map<uint32_t, AttributeInfo> m_attributeInfo;
    std::unordered_map<uint32_t, StickerKitInfo> m_stickerKitInfo; // mikkotodo string lookup?
    std::unordered_map<uint32_t, PaintKitInfo> m_paintKitInfo; // mikkotodo string lookup?
    std::unordered_map<uint32_t, MusicDefinitionInfo> m_musicDefinitionInfo; // mikkotodo string lookup?

    std::unordered_map<std::string, LootList> m_lootLists;
    std::unordered_map<uint32_t, const LootList &> m_revolvingLootLists;
};
