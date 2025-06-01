#include "stdafx.h"
#include "item_schema.h"
#include "config.h"
#include "keyvalue.h"
#include "gc_const_csgo.h" // mikkotodo remove?

// ideally this would get parsed from the item schema...
static uint32_t ItemRarityFromString(std::string_view name)
{
    const std::pair<std::string_view, uint32_t> rarityNames[] = {
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

AttributeInfo::AttributeInfo(const KeyValue &key)
{
    std::string_view type = key.GetString("attribute_type");
    if (type.size())
    {
        if (type == "float")
        {
            m_type = AttributeType::Float;
        }
        else if (type == "uint32")
        {
            m_type = AttributeType::Uint32;
        }
        else if (type == "string")
        {
            m_type = AttributeType::String;
        }
        else
        {
            // not supported, fall back to float
            Platform::Print("Unsupported attribute type %s\n", std::string{ type }.c_str());
            m_type = AttributeType::Float;
        }
    }
    else
    {
        bool integer = key.GetNumber<int>("stored_as_integer");
        m_type = integer ? AttributeType::Uint32 : AttributeType::Float;
    }
}

ItemInfo::ItemInfo(uint32_t defIndex)
    : m_defIndex{ defIndex }
    , m_rarity{ ItemSchema::RarityCommon }
    , m_quality{ ItemSchema::QualityUnique }
    , m_supplyCrateSeries{ 0 }
{
    // RecursiveParseItem parses the rest
}

PaintKitInfo::PaintKitInfo(const KeyValue &key)
    : m_defIndex{ FromString<uint32_t>(key.Name()) }
    , m_rarity{ ItemSchema::RarityCommon } // rarity is not stored here, set it in ParsePaintKitRarities
{
    m_minFloat = key.GetNumber<float>("wear_remap_min", 0.0f);
    m_maxFloat = key.GetNumber<float>("wear_remap_max", 1.0f);
}

StickerKitInfo::StickerKitInfo(const KeyValue &key)
    : m_defIndex{ FromString<uint32_t>(key.Name()) }
    , m_rarity{ ItemSchema::RarityDefault } // mikkotodo revisit... currently using item rarity if this is default
{
    std::string_view rarity = key.GetString("item_rarity");
    if (rarity.size())
    {
        m_rarity = ItemRarityFromString(rarity);
    }
}

MusicDefinitionInfo::MusicDefinitionInfo(const KeyValue &key)
    : m_defIndex{ FromString<uint32_t>(key.Name()) }
{
    assert(m_defIndex);
}

uint32_t LootListItem::CaseRarity() const
{
    if (quality == ItemSchema::QualityUnusual)
    {
        return ItemSchema::RarityUnusual;
    }

    return rarity;
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

float ItemSchema::AttributeFloat(const CSOEconItemAttribute *attribute) const
{
    auto it = m_attributeInfo.find(attribute->def_index());
    if (it == m_attributeInfo.end())
    {
        assert(false);
        return 0;
    }

    switch (it->second.m_type)
    {
    case AttributeType::Float:
        return *reinterpret_cast<const float *>(attribute->value_bytes().data());

    case AttributeType::Uint32:
        return *reinterpret_cast<const uint32_t *>(attribute->value_bytes().data());

    case AttributeType::String:
        return FromString<float>(attribute->value_bytes());

    default:
        assert(false);
        return 0;
    }
}

uint32_t ItemSchema::AttributeUint32(const CSOEconItemAttribute *attribute) const
{
    auto it = m_attributeInfo.find(attribute->def_index());
    if (it == m_attributeInfo.end())
    {
        assert(false);
        return 0;
    }

    switch (it->second.m_type)
    {
    case AttributeType::Float:
        return *reinterpret_cast<const float *>(attribute->value_bytes().data());

    case AttributeType::Uint32:
        return *reinterpret_cast<const uint32_t *>(attribute->value_bytes().data());

    case AttributeType::String:
        return FromString<uint32_t>(attribute->value_bytes());

    default:
        assert(false);
        return 0;
    }
}

std::string ItemSchema::AttributeString(const CSOEconItemAttribute *attribute) const
{
    auto it = m_attributeInfo.find(attribute->def_index());
    if (it == m_attributeInfo.end())
    {
        assert(false);
        return {};
    }

    switch (it->second.m_type)
    {
    case AttributeType::Float:
        return std::to_string(*reinterpret_cast<const float *>(attribute->value_bytes().data()));

    case AttributeType::Uint32:
        return std::to_string(*reinterpret_cast<const uint32_t *>(attribute->value_bytes().data()));

    case AttributeType::String:
        return attribute->value_bytes();

    default:
        assert(false);
        return {};
    }
}

bool ItemSchema::SetAttributeFloat(CSOEconItemAttribute *attribute, float value) const
{
    auto it = m_attributeInfo.find(attribute->def_index());
    if (it == m_attributeInfo.end())
    {
        assert(false);
        return false;
    }

    switch (it->second.m_type)
    {
    case AttributeType::Float:
    {
        attribute->set_value_bytes(&value, sizeof(value));
        break;
    }

    case AttributeType::Uint32:
    {
        uint32_t convert = static_cast<uint32_t>(value);
        attribute->set_value_bytes(&convert, sizeof(convert));
        break;
    }

    case AttributeType::String:
    {
        std::string convert = std::to_string(value);
        attribute->set_value_bytes(std::move(convert));
        break;
    }

    default:
        assert(false);
        return false;
    }

    return true;
}

bool ItemSchema::SetAttributeUint32(CSOEconItemAttribute *attribute, uint32_t value) const
{
    auto it = m_attributeInfo.find(attribute->def_index());
    if (it == m_attributeInfo.end())
    {
        assert(false);
        return false;
    }

    switch (it->second.m_type)
    {
    case AttributeType::Float:
    {
        float convert = static_cast<float>(value);
        attribute->set_value_bytes(&convert, sizeof(convert));
        break;
    }

    case AttributeType::Uint32:
    {
        attribute->set_value_bytes(&value, sizeof(value));
        break;
    }

    case AttributeType::String:
    {
        std::string convert = std::to_string(value);
        attribute->set_value_bytes(std::move(convert));
        break;
    }

    default:
        assert(false);
        return false;
    }

    return true;
}

bool ItemSchema::SetAttributeString(CSOEconItemAttribute *attribute, std::string_view value) const
{
    auto it = m_attributeInfo.find(attribute->def_index());
    if (it == m_attributeInfo.end())
    {
        assert(false);
        return false;
    }

    switch (it->second.m_type)
    {
    case AttributeType::Float:
    {
        float convert = FromString<float>(value);
        attribute->set_value_bytes(&convert, sizeof(convert));
        break;
    }

    case AttributeType::Uint32:
    {
        uint32_t convert = FromString<uint32_t>(value);
        attribute->set_value_bytes(&convert, sizeof(convert));
        break;
    }

    case AttributeType::String:
    {
        attribute->set_value_bytes(value.data(), value.size());
        break;
    }

    default:
        assert(false);
        return false;
    }

    return true;
}

const LootList *ItemSchema::GetCrateLootList(const CSOEconItem &crate) const
{
    auto itemSearch = m_itemInfo.find(crate.def_index());
    if (itemSearch == m_itemInfo.end())
    {
        assert(false);
        return nullptr;
    }

    assert(itemSearch->second.m_supplyCrateSeries);

    auto lootListSearch = m_revolvingLootLists.find(itemSearch->second.m_supplyCrateSeries);
    if (lootListSearch == m_revolvingLootLists.end())
    {
        assert(false);
        return nullptr;
    }

    return &lootListSearch->second;
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
        auto emplace = m_itemInfo.try_emplace(defIndex, defIndex);

        ParseItemRecursive(emplace.first->second, itemKey, prefabsKey);
    }
}

// ideally this would get parsed from the item schema...
static uint32_t ItemQualityFromString(std::string_view name)
{
    const std::pair<std::string_view, uint32_t> qualityNames[] = {
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
        info.m_name = name;
    }

    std::string_view quality = itemKey.GetString("item_quality");
    if (quality.size())
    {
        info.m_quality = ItemQualityFromString(quality);
    }

    std::string_view rarity = itemKey.GetString("item_rarity");
    if (rarity.size())
    {
        info.m_rarity = ItemRarityFromString(rarity);
    }

    const KeyValue *attributes = itemKey.GetSubkey("attributes");
    if (attributes)
    {
        const KeyValue *supplyCrateSeries = attributes->GetSubkey("set supply crate series");
        if (supplyCrateSeries)
        {
            info.m_supplyCrateSeries = supplyCrateSeries->GetNumber<uint32_t>("value");
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
        m_attributeInfo.try_emplace(defIndex, attributeKey);
    }
}

void ItemSchema::ParseStickerKits(const KeyValue *stickerKitsKey)
{
    m_stickerKitInfo.reserve(stickerKitsKey->SubkeyCount());

    for (const KeyValue &stickerKitKey : *stickerKitsKey)
    {
        std::string_view name = stickerKitKey.GetString("name");

        m_stickerKitInfo.emplace(std::piecewise_construct,
            std::forward_as_tuple(name),
            std::forward_as_tuple(stickerKitKey));
    }
}

void ItemSchema::ParsePaintKits(const KeyValue *paintKitsKey)
{
    m_paintKitInfo.reserve(paintKitsKey->SubkeyCount());

    for (const KeyValue &paintKitKey : *paintKitsKey)
    {
        std::string_view name = paintKitKey.GetString("name");

        m_paintKitInfo.emplace(std::piecewise_construct,
            std::forward_as_tuple(name),
            std::forward_as_tuple(paintKitKey));
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

        assert(paintKitInfo->m_rarity == RarityCommon);
        paintKitInfo->m_rarity = ItemRarityFromString(key.String());
    }
}

void ItemSchema::ParseMusicDefinitions(const KeyValue *musicDefinitionsKey)
{
    m_musicDefinitionInfo.reserve(musicDefinitionsKey->SubkeyCount());

    for (const KeyValue &musicDefinitionKey : *musicDefinitionsKey)
    {
        std::string_view name = musicDefinitionKey.GetString("name");

        m_musicDefinitionInfo.emplace(std::piecewise_construct,
            std::forward_as_tuple(name),
            std::forward_as_tuple(musicDefinitionKey));
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

    const std::pair<std::string_view, LootListItemType> mapNames[] = {
        { "sticker", LootListItemSticker },
        { "spray", LootListItemSpray },
        { "patch", LootListItemPatch },
        { "musickit", LootListItemMusicKit }
    };

    for (const auto &pair : mapNames)
    {
        if (pair.first == name)
        {
            return pair.second;
        }
    }

    return LootListItemPaintable;
}

void ItemSchema::ParseLootLists(const KeyValue *lootListsKey, bool unusual)
{
    m_lootLists.reserve(lootListsKey->SubkeyCount());

    for (const KeyValue &lootListKey : *lootListsKey)
    {
        auto emplace = m_lootLists.emplace(std::piecewise_construct,
            std::forward_as_tuple(lootListKey.Name()),
            std::forward_as_tuple());

        LootList &lootList = emplace.first->second;
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
    item.rarity = itemInfo->m_rarity;
    item.quality = itemInfo->m_quality;

    if (item.type == LootListItemNoAttribute)
    {
        // no attribute
    }
    else if (item.type == LootListItemSticker || item.type == LootListItemSpray || item.type == LootListItemPatch)
    {
        // the attribute is the sticker name
        item.stickerKitInfo = StickerKitInfoByName(attributeName);
        if (!item.stickerKitInfo)
        {
            Platform::Print("WARNING: No such sticker kit %s\n", std::string{ attributeName }.c_str());
            return false;
        }

        // sticker kits affect the item rarity (mikkotodo how do these work, something like PaintedItemRarity???)
        assert(itemInfo->m_rarity == 1);

        if (item.stickerKitInfo->m_rarity)
        {
            item.rarity = item.stickerKitInfo->m_rarity;
        }
    }
    else if (item.type == LootListItemMusicKit)
    {
        // the attribute is the music definition name
        item.musicDefinitionInfo = MusicDefinitionInfoByName(attributeName);
        if (!item.musicDefinitionInfo)
        {
            Platform::Print("WARNING: No such music definition %s\n", std::string{ attributeName }.c_str());
            return false;
        }
    }
    else
    {
        // probably a paint kit
        assert(item.type == LootListItemPaintable);
        item.paintKitInfo = PaintKitInfoByName(attributeName);
        if (!item.paintKitInfo)
        {
            assert(false);
            Platform::Print("WARNING: No such paint kit %s\n", std::string{ attributeName }.c_str());
            return false;
        }

        // paint kits affect the item rarity
        item.rarity = PaintedItemRarity(itemInfo->m_rarity, item.paintKitInfo->m_rarity);
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
        if (info.m_name == name)
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

MusicDefinitionInfo *ItemSchema::MusicDefinitionInfoByName(std::string_view name)
{
    auto it = m_musicDefinitionInfo.find(std::string{ name });
    if (it == m_musicDefinitionInfo.end())
    {
        assert(false);
        return nullptr;
    }

    return &it->second;
}
