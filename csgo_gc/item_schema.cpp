#include "stdafx.h"
#include "item_schema.h"
#include "keyvalue.h"
#include "gc_const_csgo.h" // mikkotodo remove?
#include "random.h"

#include <tuple>

template<typename T, typename... Args>
inline T &MapAlloc(std::unordered_map<std::string, T> &map, std::string_view key)
{
    auto result = map.emplace(std::piecewise_construct,
        std::forward_as_tuple(key),
        std::forward_as_tuple());

    assert(result.second);
    return result.first->second;
}

template<typename T, typename... Args>
inline T &MapAlloc(std::unordered_map<uint32_t, T> &map, uint32_t key)
{
    auto result = map.emplace(std::piecewise_construct,
        std::forward_as_tuple(key),
        std::forward_as_tuple());

    assert(result.second);
    return result.first->second;
}

ItemSchema::ItemSchema()
{
    KeyValue itemSchema{ "root" };
    if (!itemSchema.ParseFromFile("csgo/scripts/items/items_game.txt"))
    {
        assert(false);
        return;
    }

    const KeyValue *itemsGame = itemSchema.GetSubkey("items_game");
    if (!itemsGame)
    {
        assert(false);
        return;
    }

    const KeyValue *itemsKey = itemsGame->GetSubkey("items");
    if (itemsKey)
    {
        ParseItems(itemsKey, itemsGame->GetSubkey("prefabs"));
    }

    const KeyValue *attributesKey = itemsGame->GetSubkey("attributes");
    if (attributesKey)
    {
        ParseAttributes(attributesKey);
    }

    const KeyValue *stickerKitsKey = itemsGame->GetSubkey("sticker_kits");
    if (stickerKitsKey)
    {
        ParseStickerKits(stickerKitsKey);
    }

    const KeyValue *paintKitsKey = itemsGame->GetSubkey("paint_kits");
    if (paintKitsKey)
    {
        ParsePaintKits(paintKitsKey);
    }

    const KeyValue *paintKitsRarityKey = itemsGame->GetSubkey("paint_kits_rarity");
    if (paintKitsRarityKey)
    {
        ParsePaintKitRarities(paintKitsRarityKey);
    }

    const KeyValue *musicDefinitionsKey = itemsGame->GetSubkey("music_definitions");
    if (musicDefinitionsKey)
    {
        ParseMusicDefinitions(musicDefinitionsKey);
    }

    // unusual loot lists are not included in client_loot_lists
    // we need to parse these after items and paint kits but before client_loot_lists
    {
        KeyValue unusualLootLists{ "unusual_loot_lists" };

        if (unusualLootLists.ParseFromFile("csgo_gc/unusual_loot_lists.txt"))
        {
            ParseLootLists(&unusualLootLists, true);
        }
        else
        {
            // no knives sorry
            assert(false);
        }
    }

    const KeyValue *lootListsKey = itemsGame->GetSubkey("client_loot_lists");
    if (lootListsKey)
    {
        ParseLootLists(lootListsKey, false);
    }

    const KeyValue *revolvingLootListsKey = itemsGame->GetSubkey("revolving_loot_lists");
    if (revolvingLootListsKey)
    {
        ParseRevolvingLootLists(revolvingLootListsKey);
    }
}

bool ItemSchema::AttributeStoredAsInteger(uint32_t defIndex) const
{
    auto it = m_attributeInfo.find(defIndex);
    if (it != m_attributeInfo.end())
    {
        return it->second.storedAsInteger;
    }

    return false;
}

int ItemSchema::AttributeValueInt(const CSOEconItemAttribute &attribute) const
{
    assert(attribute.value_bytes().size() == 4);

    auto it = m_attributeInfo.find(attribute.def_index());
    if (it == m_attributeInfo.end())
    {
        assert(false);
        return 0;
    }

    if (it->second.storedAsInteger)
    {
        return *reinterpret_cast<const int *>(attribute.value_bytes().data());
    }
    else
    {
        return *reinterpret_cast<const float *>(attribute.value_bytes().data());
    }
}

float ItemSchema::AttributeValueFloat(const CSOEconItemAttribute &attribute) const
{
    assert(attribute.value_bytes().size() == 4);

    auto it = m_attributeInfo.find(attribute.def_index());
    if (it == m_attributeInfo.end())
    {
        assert(false);
        return 0;
    }

    if (it->second.storedAsInteger)
    {
        return *reinterpret_cast<const int *>(attribute.value_bytes().data());
    }
    else
    {
        return *reinterpret_cast<const float *>(attribute.value_bytes().data());
    }
}

bool ItemSchema::EconItemFromLootListItem(const LootListItem &lootListItem, CSOEconItem &item, GenerateStatTrak generateStatTrak)
{
    bool statTrak;

    switch (generateStatTrak)
    {
    case GenerateStatTrak::Yes:
        statTrak = true;
        break;

    case GenerateStatTrak::Maybe:
        statTrak = (g_random.Uint32(1, 10) == 1);
        break;

  default:
        statTrak = false;
        break;
    }

    // NOTE: unusual stattraks only valid below id 1000
    if (statTrak
        && lootListItem.quality == QualityUnusual
        && lootListItem.itemInfo->defIndex >= 1000)
    {
        statTrak = false;
    }

    // stattrak affects quality
    uint32_t quality = lootListItem.quality;

    // but unusual overrides strange
    if (statTrak && lootListItem.quality != QualityUnusual)
    {
        quality = QualityStrange;
    }

    assert(lootListItem.rarity);

    item.set_inventory(InventoryUnacknowledged(UnacknowledgedFoundInCrate));
    item.set_def_index(lootListItem.itemInfo->defIndex);
    item.set_quantity(1);
    item.set_level(1); // mikkotodo parse from item
    item.set_quality(quality);
    item.set_flags(0);
    item.set_origin(ItemOriginCrate);
    item.set_in_use(false);
    item.set_rarity(lootListItem.rarity);

    if (lootListItem.type == LootListItemSticker)
    {
        // mikkotodo anything else?
        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerId0);
        SetAttributeValueInt(*attribute, lootListItem.attribute.stickerKitIndex);
    }
    else if (lootListItem.type == LootListItemSpray)
    {
        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerId0);
        SetAttributeValueInt(*attribute, lootListItem.attribute.stickerKitIndex);

        // add AttributeSpraysRemaining when it's unsealed (mikkotodo how does the real gc do this)

        attribute = item.add_attribute();
        attribute->set_def_index(AttributeSprayTintId);
        SetAttributeValueInt(*attribute, g_random.Uint32(GraffitiTintMin, GraffitiTintMax));
    }
    else if (lootListItem.type == LootListItemPatch)
    {
        // mikkotodo anything else?
        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeStickerId0);
        SetAttributeValueInt(*attribute, lootListItem.attribute.stickerKitIndex);
    }
    else if (lootListItem.type == LootListItemMusicKit)
    {
        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeMusicId);
        SetAttributeValueInt(*attribute, lootListItem.attribute.musicDefinitionIndex);
    }
    else if (lootListItem.type == LootListItemPaintable)
    {
        const PaintKitInfo *paintKitInfo = lootListItem.attribute.paintKitInfo;

        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeTexturePrefab);
        SetAttributeValueInt(*attribute, paintKitInfo->defIndex);

        attribute = item.add_attribute();
        attribute->set_def_index(AttributeTextureSeed);
        SetAttributeValueInt(*attribute, g_random.Uint32(0, 1000));

        // mikkotodo how does the float distribution work?
        attribute = item.add_attribute();
        attribute->set_def_index(AttributeTextureWear);
        SetAttributeValueFloat(*attribute, g_random.Float(paintKitInfo->minFloat, paintKitInfo->maxFloat));
    }
    else if (lootListItem.type == LootListItemNoAttribute)
    {
        // nothing
    }
    else
    {
        assert(false);
    }

    if (statTrak)
    {
        assert((lootListItem.type == LootListItemMusicKit) || (lootListItem.type == LootListItemPaintable));

        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(AttributeKillEater);
        SetAttributeValueInt(*attribute, 0);

        // mikkotodo fix magic
        int scoreType = (lootListItem.type == LootListItemMusicKit) ? 1 : 0;

        attribute = item.add_attribute();
        attribute->set_def_index(AttributeKillEaterScoreType);
        SetAttributeValueInt(*attribute, scoreType);

    }

    return true;
}

const LootListItem &ItemSchema::SelectItemFromLists(const std::vector<const LootList *> &lists)
{
    // mikkotodo implement probabilities
    size_t listIndex = g_random.RandomIndex(lists.size());
    const LootList *list = lists[listIndex];

    assert(list->items.size() && !list->subLists.size());

    size_t itemIndex = g_random.RandomIndex(list->items.size());
    return list->items[itemIndex];
}

static bool ListContainsUnusuals(const LootList &lootList)
{
    // ONLY check top level sub lists
    for (const LootList *other : lootList.subLists)
    {
        if (other->isUnusual)
        {
            return true;
        }
    }

    return false;
}

bool ItemSchema::SelectItemFromCrate(const CSOEconItem &crate, CSOEconItem &item)
{
    auto itemSearch = m_itemInfo.find(crate.def_index());
    if (itemSearch == m_itemInfo.end())
    {
        assert(false);
        return false;
    }

    assert(itemSearch->second.supplyCrateSeries);

    auto lootListSearch = m_revolvingLootLists.find(itemSearch->second.supplyCrateSeries);
    if (lootListSearch == m_revolvingLootLists.end())
    {
        assert(false);
        return false;
    }

    const LootList &lootList = lootListSearch->second;
    assert(lootList.subLists.empty() != lootList.items.empty());

    // stattrak def
    GenerateStatTrak generateStatTrak = GenerateStatTrak::No;
    if (lootList.willProduceStatTrak)
    {
        generateStatTrak = GenerateStatTrak::Yes;
    }
    else if (ListContainsUnusuals(lootList))
    {
        generateStatTrak = GenerateStatTrak::Maybe;
    }

    if (lootList.subLists.size())
    {
        const LootListItem &lootListItem = SelectItemFromLists(lootList.subLists);

        // mikkotodo stattrak randomization
        return EconItemFromLootListItem(lootListItem, item, generateStatTrak);
    }
    else if (lootList.items.size())
    {
        size_t index = g_random.RandomIndex(lootList.items.size());
        const LootListItem &lootListItem = lootList.items[index];

        return EconItemFromLootListItem(lootListItem, item, generateStatTrak);
    }
    else
    {
        assert(false);
        return false;
    }

    return true;
}

void ItemSchema::ParseItems(const KeyValue *itemsKey, const KeyValue *prefabsKey)
{
    m_itemInfo.reserve(itemsKey->SubkeyCount());

    for (const KeyValue &itemKey : *itemsKey)
    {
        if (itemKey.Name() == "default")
        {
            // ignore this
            continue;
        }

        uint32_t defIndex = FromString<uint32_t>(itemKey.Name());
        ItemInfo &info = MapAlloc(m_itemInfo, defIndex);
        info.defIndex = defIndex;

        ParseItemRecursive(info, itemKey, prefabsKey);
    }
}

// ideally this would get parsed from the item schema...
static uint32_t ItemQualityFromString(std::string_view name)
{
    const std::pair<std::string_view, uint32_t> qualityNames[] =
    {
        { "normal", ItemSchema::QualityNormal },
        { "genuine", ItemSchema::QualityGenuine },
        { "vintage", ItemSchema::QualityVintage },
        { "unusual", ItemSchema::QualityUnusual },
        { "unique", ItemSchema::QualityUnique },
        { "community", ItemSchema::QualityCommunity },
        { "developer", ItemSchema::QualityDeveloper },
        { "selfmade", ItemSchema::QualitySelfmade },
        { "customized", ItemSchema::QualityCustomized },
        { "strange", ItemSchema::QualityStrange },
        { "completed", ItemSchema::QualityCompleted },
        { "haunted", ItemSchema::QualityHaunted },
        { "tournament", ItemSchema::QualityTournament },
    };

    for (const auto &pair : qualityNames)
    {
        if (pair.first == name)
        {
            return pair.second;
        }
    }

    assert(false);
    return ItemSchema::QualityUnique; // i guess???
}

// ideally this would get parsed from the item schema...
static uint32_t ItemRarityFromString(std::string_view name)
{
    const std::pair<std::string_view, uint32_t> rarityNames[] =
    {
        { "default", ItemSchema::RarityDefault },
        { "common", ItemSchema::RarityCommon },
        { "uncommon", ItemSchema::RarityUncommon },
        { "rare", ItemSchema::RarityRare },
        { "mythical", ItemSchema::RarityMythical },
        { "legendary", ItemSchema::RarityLegendary },
        { "ancient", ItemSchema::RarityAncient },
        { "immortal", ItemSchema::RarityImmortal },
        { "unusual", ItemSchema::RarityUnusual },
    };

    for (const auto &pair : rarityNames)
    {
        if (pair.first == name)
        {
            return pair.second;
        }
    }

    assert(false);
    return ItemSchema::RarityCommon;
}

void ItemSchema::ParseItemRecursive(ItemInfo &info, const KeyValue &itemKey, const KeyValue *prefabsKey)
{
    std::string_view prefabName = itemKey.GetString("prefab");
    if (prefabName.size() && prefabsKey)
    {
        const KeyValue *prefabKey = prefabsKey->GetSubkey(prefabName);
        if (prefabKey)
        {
            ParseItemRecursive(info, *prefabKey, prefabsKey);
        }
    }

    std::string_view name = itemKey.GetString("name");
    if (name.size())
    {
        info.name = name;
    }

    std::string_view quality = itemKey.GetString("item_quality");
    if (quality.size())
    {
        info.quality = ItemQualityFromString(quality);
    }

    std::string_view rarity = itemKey.GetString("item_rarity");
    if (rarity.size())
    {
        info.rarity = ItemRarityFromString(rarity);
    }

    const KeyValue *attributes = itemKey.GetSubkey("attributes");
    if (attributes)
    {
        const KeyValue *supplyCrateSeries = attributes->GetSubkey("set supply crate series");
        if (supplyCrateSeries)
        {
            info.supplyCrateSeries = supplyCrateSeries->GetNumber<uint32_t>("value");
        }
    }
}

void ItemSchema::ParseAttributes(const KeyValue *attributesKey)
{
    m_attributeInfo.reserve(attributesKey->SubkeyCount());
    
    for (const KeyValue &attributeKey : *attributesKey)
    {
        uint32_t defIndex = FromString<uint32_t>(attributeKey.Name());
        assert(defIndex);

        AttributeInfo &info = MapAlloc(m_attributeInfo, defIndex);
        info.storedAsInteger = attributeKey.GetNumber<int>("stored_as_integer") ? true : false;
    }
}

void ItemSchema::ParseStickerKits(const KeyValue *stickerKitsKey)
{
    m_stickerKitInfo.reserve(stickerKitsKey->SubkeyCount());

    for (const KeyValue &stickerKitKey : *stickerKitsKey)
    {
        uint32_t defIndex = FromString<uint32_t>(stickerKitKey.Name());
        std::string_view name = stickerKitKey.GetString("name");

        StickerKitInfo &info = MapAlloc(m_stickerKitInfo, name);
        info.defIndex = defIndex;

        std::string_view rarity = stickerKitKey.GetString("item_rarity");
        if (rarity.size())
        {
            info.rarity = ItemRarityFromString(rarity);
        }
        else
        {
            info.rarity = 0;
        }
    }
}

void ItemSchema::ParsePaintKits(const KeyValue *paintKitsKey)
{
    m_paintKitInfo.reserve(paintKitsKey->SubkeyCount());

    for (const KeyValue &paintKitKey : *paintKitsKey)
    {
        uint32_t defIndex = FromString<uint32_t>(paintKitKey.Name());
        std::string_view name = paintKitKey.GetString("name");

        PaintKitInfo &info = MapAlloc(m_paintKitInfo, name);
        info.defIndex = defIndex;

        // rarity gets set in ParsePaintKitRarities
        info.minFloat = paintKitKey.GetNumber<float>("wear_remap_min", 0.0f);
        info.maxFloat = paintKitKey.GetNumber<float>("wear_remap_max", 1.0f);
    }
}

void ItemSchema::ParsePaintKitRarities(const KeyValue *raritiesKey)
{
    for (const KeyValue &key : *raritiesKey)
    {
        PaintKitInfo *paintKitInfo = PaintKitInfoByName(key.Name());
        if (!paintKitInfo)
        {
            //assert(false);
            //Platform::Print("No such paint kit '%s'!!!\n", std::string{ key.Name() }.c_str());
            continue;
        }

        assert(paintKitInfo->rarity == 0);
        paintKitInfo->rarity = ItemRarityFromString(key.String());
    }
}

void ItemSchema::ParseMusicDefinitions(const KeyValue *musicDefinitionsKey)
{
    m_musicDefinitionInfo.reserve(musicDefinitionsKey->SubkeyCount());

    for (const KeyValue &musicDefinitionKey : *musicDefinitionsKey)
    {
        uint32_t defIndex = FromString<uint32_t>(musicDefinitionKey.Name());
        std::string_view name = musicDefinitionKey.GetString("name");

        assert(defIndex);

        MusicDefinitionInfo &info = MapAlloc(m_musicDefinitionInfo, name);
        info.defIndex = defIndex;
    }
}

static void ParseAttributeAndItemName(std::string_view input, std::string_view &attribute, std::string_view &item)
{
    // fallbacks (mikkotodo unfuck)
    attribute = {};
    item = input;

    if (input[0] != '[')
        return;

    size_t attribEnd = input.find(']', 1);
    if (attribEnd == std::string_view::npos)
    {
        assert(false);
        return;
    }

    attribute = input.substr(1, attribEnd - 1);
    item = input.substr(attribEnd + 1);

    assert(attribute.size() && item.size());
}

static LootListItemType LootListItemTypeFromName(std::string_view name, std::string_view attributeName)
{
    if (attributeName.empty())
    {
        return LootListItemNoAttribute;
    }

    // should match LootListItemType
    const std::string_view names[] =
    {
        "sticker",
        "spray",
        "patch",
        "musickit"
    };

    for (size_t i = 0; i < std::size(names); i++)
    {
        if (names[i] == name)
        {
            return static_cast<LootListItemType>(i);
        }
    }

    return LootListItemPaintable;
}

void ItemSchema::ParseLootLists(const KeyValue *lootListsKey, bool unusual)
{
    m_lootLists.reserve(lootListsKey->SubkeyCount());

    for (const KeyValue &lootListKey : *lootListsKey)
    {
        LootList &lootList = MapAlloc(m_lootLists, lootListKey.Name());
        lootList.isUnusual = unusual;

        for (const KeyValue &entryKey : lootListKey)
        {
            std::string_view entryName = entryKey.Name();

            // check for options that we ignore
            if (entryName == "will_produce_stattrak")
            {
                lootList.willProduceStatTrak = true;
                continue;
            }

            // check for options that we ignore
            if (entryName == "all_entries_as_additional_drops"
                || entryName == "contains_patches_representing_organizations"
                || entryName == "contains_stickers_autographed_by_proplayers"
                || entryName == "contains_stickers_representing_organizations"
                || entryName == "limit_description_to_number_rnd"
                || entryName == "public_list_contents")
            {
                continue;
            }

            std::string entryNameKey{ entryKey.Name() };

            // check if it's another loot list
            auto listSearch = m_lootLists.find(entryNameKey);
            if (listSearch != m_lootLists.end())
            {
                lootList.subLists.push_back(&listSearch->second);
                continue;
            }

            // check for an item
            LootListItem item;
            if (ParseLootListItem(item, entryName))
            {
                if (unusual)
                {
                    // override the quality here...
                    item.quality = QualityUnusual;
                }

                lootList.items.push_back(item);
            }
            else
            {
                // what the fuck is this...
                Platform::Print("Unhandled loot list entry %s!!!!\n", entryNameKey.c_str());
            }
        }
    }
}

static uint32_t PaintedItemRarity(uint32_t itemRarity, uint32_t paintKitRarity)
{
    int rarity = (itemRarity - 1) + paintKitRarity;
    if (rarity < 0)
    {
        return 0;
    }

    if (rarity > ItemSchema::RarityAncient)
    {
        if (paintKitRarity == ItemSchema::RarityImmortal)
        {
            return ItemSchema::RarityImmortal;
        }

        return ItemSchema::RarityAncient;
    }

    return rarity;
}

// mikkotodo rewrite this function
bool ItemSchema::ParseLootListItem(LootListItem &item, std::string_view name)
{
    // check for an attribute + item combo
    std::string_view attributeName, itemName;
    ParseAttributeAndItemName(name, attributeName, itemName);

    const ItemInfo *itemInfo = ItemInfoByName(itemName);
    if (!itemInfo)
    {
        Platform::Print("No such item %s!!!\n", std::string{ itemName }.c_str());
        return false;
    }

    item.itemInfo = itemInfo;
    item.type = LootListItemTypeFromName(itemName, attributeName);

    // until proven otherwise...
    item.rarity = itemInfo->rarity;
    item.quality = itemInfo->quality;

    if (item.type == LootListItemNoAttribute)
    {
        // no attribute
        item.attribute.paintKitInfo = nullptr;
    }
    else if (item.type == LootListItemSticker || item.type == LootListItemSpray || item.type == LootListItemPatch)
    {
        // the attribute is the sticker name
        const StickerKitInfo *stickerKitInfo = StickerKitInfoByName(attributeName);
        if (!stickerKitInfo) // mikkotodo 0 can be valid though??? restructure
        {
            Platform::Print("WARNING: No such sticker kit %s\n", std::string{ attributeName }.c_str());
            return false;
        }

        item.attribute.stickerKitIndex = stickerKitInfo->defIndex;

        // sticker kits affect the item rarity (mikkotodo how do these work, something like PaintedItemRarity???)
        assert(itemInfo->rarity == 1);
        item.rarity = stickerKitInfo->rarity ? stickerKitInfo->rarity : itemInfo->rarity;
    }
    else if (item.type == LootListItemMusicKit)
    {
        // the attribute is the music definition name
        item.attribute.musicDefinitionIndex = MusicDefinitionIndexByName(attributeName);
        if (!item.attribute.musicDefinitionIndex) // mikkotodo 0 can be valid though??? restructure
        {
            Platform::Print("WARNING: No such music definition %s\n", std::string{ attributeName }.c_str());
            return false;
        }
    }
    else
    {
        // probably a paint kit
        assert(item.type == LootListItemPaintable);
        const PaintKitInfo *paintKitInfo = PaintKitInfoByName(attributeName);
        if (!paintKitInfo)
        {
            assert(false);
            Platform::Print("WARNING: No such paint kit %s\n", std::string{ attributeName }.c_str());
            return false;
        }

        item.attribute.paintKitInfo = paintKitInfo;

        // paint kits affect the item rarity
        item.rarity = PaintedItemRarity(itemInfo->rarity, paintKitInfo->rarity);
    }

    return true;
}

void ItemSchema::ParseRevolvingLootLists(const KeyValue *revolvingLootListsKey)
{
    m_revolvingLootLists.reserve(revolvingLootListsKey->SubkeyCount());

    for (const KeyValue &revolvingLootListKey : *revolvingLootListsKey)
    {
        uint32_t index = FromString<uint32_t>(revolvingLootListKey.Name());
        assert(index);

        // ugh
        std::string lootListName = std::string{ revolvingLootListKey.String() };

        auto it = m_lootLists.find(lootListName);
        if (it == m_lootLists.end())
        {
            //Platform::Print("Ignoring revolving loot list %s\n", lootListName.c_str());
            continue;
        }

        m_revolvingLootLists.try_emplace(index, it->second);
    }
}

ItemInfo *ItemSchema::ItemInfoByName(std::string_view name)
{
    for (auto &pair : m_itemInfo)
    {
        const ItemInfo &info = pair.second;
        if (info.name == name)
        {
            return &pair.second;
        }
    }

    assert(false);
    return nullptr;
}

StickerKitInfo *ItemSchema::StickerKitInfoByName(std::string_view name)
{
    auto it = m_stickerKitInfo.find(std::string{ name });
    if (it == m_stickerKitInfo.end())
    {
        assert(false);
        return nullptr;
    }

    return &it->second;
}

PaintKitInfo *ItemSchema::PaintKitInfoByName(std::string_view name)
{
    auto it = m_paintKitInfo.find(std::string{ name });
    if (it == m_paintKitInfo.end())
    {
        //assert(false);
        return nullptr;
    }

    return &it->second;
}

uint32_t ItemSchema::MusicDefinitionIndexByName(std::string_view name)
{
    auto it = m_musicDefinitionInfo.find(std::string{ name });
    if (it == m_musicDefinitionInfo.end())
    {
        assert(false);
        return 0;
    }

    return it->second.defIndex;
}

void ItemSchema::SetAttributeValueInt(CSOEconItemAttribute &attribute, int value) const
{
    if (AttributeStoredAsInteger(attribute.def_index()))
    {
        attribute.set_value_bytes(&value, sizeof(value));
    }
    else
    {
        float valueFloat = static_cast<float>(value);
        attribute.set_value_bytes(&valueFloat, sizeof(valueFloat));
    }
}

void ItemSchema::SetAttributeValueFloat(CSOEconItemAttribute &attribute, float value) const
{
    if (AttributeStoredAsInteger(attribute.def_index()))
    {
        int valueInt = static_cast<int>(value);
        attribute.set_value_bytes(&valueInt, sizeof(valueInt));
    }
    else
    {
        attribute.set_value_bytes(&value, sizeof(value));
    }
}
