#include "stdafx.h"
#include "item_schema.h"
#include "config.h"
#include "item.h"
#include "keyvalue.h"
#include "random.h"

// ideally this would get parsed from the item schema...
static Rarity ItemRarityFromString(std::string_view name)
{
    const std::pair<std::string_view, Rarity> rarityNames[] = {
        { "default", Rarity::Default },
        { "common", Rarity::Common },
        { "uncommon", Rarity::Uncommon },
        { "rare", Rarity::Rare },
        { "mythical", Rarity::Mythical },
        { "legendary", Rarity::Legendary },
        { "ancient", Rarity::Ancient },
        { "immortal", Rarity::Immortal },
        { "unusual", Rarity::Unusual },
    };

    for (const auto &pair : rarityNames)
    {
        if (pair.first == name)
        {
            return pair.second;
        }
    }

    assert(false);
    return Rarity::Common;
}

// ideally this would get parsed from the item schema...
static Quality ItemQualityFromString(std::string_view name)
{
    const std::pair<std::string_view, Quality> qualityNames[] = {
        { "normal", Quality::Normal },
        { "genuine", Quality::Genuine },
        { "vintage", Quality::Vintage },
        { "unusual", Quality::Unusual },
        { "unique", Quality::Unique },
        { "community", Quality::Community },
        { "developer", Quality::Developer },
        { "selfmade", Quality::Selfmade },
        { "customized", Quality::Customized },
        { "strange", Quality::Strange },
        { "completed", Quality::Completed },
        { "haunted", Quality::Haunted },
        { "tournament", Quality::Tournament },
    };

    for (const auto &pair : qualityNames)
    {
        if (pair.first == name)
        {
            return pair.second;
        }
    }

    assert(false);
    return Quality::Unique; // i guess???
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

ItemInfo::ItemInfo(ItemDefIndex defIndex)
    : m_defIndex{ defIndex }
    , m_rarity{ Rarity::Common }
    , m_quality{ Quality::Normal }
    , m_level{ 1 }
    , m_supplyCrateSeries{ 0 }
    , m_isCoupon{ false }
    , m_willProduceStatTrak{ false }
{
    // RecursiveParseItem parses the rest
}

PaintKitInfo::PaintKitInfo(const KeyValue &key)
    : m_defIndex{ FromString<uint32_t>(key.Name()) }
    , m_rarity{ Rarity::Common } // rarity is not set here, done in ParsePaintKitRarities
{
    m_minFloat = key.GetNumber<float>("wear_remap_min", 0.0f);
    m_maxFloat = key.GetNumber<float>("wear_remap_max", 1.0f);
}

StickerKitInfo::StickerKitInfo(const KeyValue &key)
    : m_defIndex{ FromString<uint32_t>(key.Name()) }
    , m_rarity{ Rarity::Default } // mikkotodo revisit... currently using item rarity if this is default
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

Rarity LootListItem::CaseRarity() const
{
    if (quality == Quality::Unusual)
    {
        return Rarity::Unusual;
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

static std::string DecodeAttributeString(std::string_view data)
{
    CAttribute_String attribute;
    if (!attribute.ParseFromString(data))
    {
        assert(false);
        return {};
    }

    return attribute.value();
}

static std::string EncodeAttributeString(std::string_view string)
{
    CAttribute_String attribute;
    attribute.set_value(string);
    return attribute.SerializeAsString();
}

float ItemSchema::AttributeFloat(const CSOEconItemAttribute *attribute) const
{
    auto it = m_attributeInfo.find(ToEnum<AttributeDefIndex>(attribute->def_index()));
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
        return FromString<float>(DecodeAttributeString(attribute->value_bytes()));

    default:
        assert(false);
        return 0;
    }
}

uint32_t ItemSchema::AttributeUint32(const CSOEconItemAttribute *attribute) const
{
    auto it = m_attributeInfo.find(ToEnum<AttributeDefIndex>(attribute->def_index()));
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
        return FromString<uint32_t>(DecodeAttributeString(attribute->value_bytes()));

    default:
        assert(false);
        return 0;
    }
}

std::string ItemSchema::AttributeString(const CSOEconItemAttribute *attribute) const
{
    auto it = m_attributeInfo.find(ToEnum<AttributeDefIndex>(attribute->def_index()));
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
        return DecodeAttributeString(attribute->value_bytes());

    default:
        assert(false);
        return {};
    }
}

bool ItemSchema::SetAttributeFloat(CSOEconItemAttribute *attribute, float value) const
{
    auto it = m_attributeInfo.find(ToEnum<AttributeDefIndex>(attribute->def_index()));
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
        attribute->set_value_bytes(EncodeAttributeString(convert));
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
    auto it = m_attributeInfo.find(ToEnum<AttributeDefIndex>(attribute->def_index()));
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
        attribute->set_value_bytes(EncodeAttributeString(convert));
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
    auto it = m_attributeInfo.find(ToEnum<AttributeDefIndex>(attribute->def_index()));
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
        attribute->set_value_bytes(EncodeAttributeString(value));
        break;
    }

    default:
        assert(false);
        return false;
    }

    return true;
}

AttributeType ItemSchema::GetAttributeType(AttributeDefIndex defIndex) const
{
    auto it = m_attributeInfo.find(defIndex);
    if (it == m_attributeInfo.end())
    {
        assert(false);
        return AttributeType::Float;
    }

    return it->second.m_type;
}

const LootList *ItemSchema::GetCrateLootList(ItemDefIndex crateDefIndex) const
{
    auto itemSearch = m_itemInfo.find(crateDefIndex);
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

bool ItemSchema::ItemDescForLootListItem(Random &random,
    const LootListItem &lootListItem,
    bool statTrak,
    ItemOrigin origin,
    UnacknowledgedType unacknowledgedType,
    ItemDesc &desc) const
{
    if (!GetItemDesc(lootListItem.itemInfo->m_defIndex, origin, unacknowledgedType, desc))
    {
        assert(false);
        return false;
    }

    // quality override, stattrak makes it strange if it's not an unusual
    if (statTrak && lootListItem.quality != Quality::Unusual)
    {
        desc.quality = Quality::Strange;
    }
    else
    {
        desc.quality = lootListItem.quality;
    }

    // rarity override
    assert(lootListItem.rarity >= Rarity::Common && lootListItem.rarity <= Rarity::Immortal);
    desc.rarity = lootListItem.rarity;

    // setup type specficic attributes

    if (lootListItem.type == LootListItemSticker || lootListItem.type == LootListItemPatch)
    {
        // mikkotodo anything else?
        desc.attributes.emplace_back(
            AttributeDefIndex::StickerId0,
            lootListItem.stickerKitInfo->m_defIndex);
    }
    else if (lootListItem.type == LootListItemSpray)
    {
        desc.attributes.emplace_back(
            AttributeDefIndex::StickerId0,
            lootListItem.stickerKitInfo->m_defIndex);

        // add AttributeSpraysRemaining when it's unsealed (mikkotodo how does the real gc do this)

        desc.attributes.emplace_back(
            AttributeDefIndex::SprayTintId,
            random.Integer(FromEnum(GraffitiTint::Min), FromEnum(GraffitiTint::Max)));
    }
    else if (lootListItem.type == LootListItemMusicKit)
    {
        desc.attributes.emplace_back(
            AttributeDefIndex::MusicId,
            lootListItem.musicDefinitionInfo->m_defIndex);
    }
    else if (lootListItem.type == LootListItemPaintable)
    {
        const PaintKitInfo *paintKitInfo = lootListItem.paintKitInfo;

        desc.attributes.emplace_back(
            AttributeDefIndex::TexturePrefab,
            static_cast<float>(paintKitInfo->m_defIndex));

        desc.attributes.emplace_back(
            AttributeDefIndex::TextureSeed,
            static_cast<float>(random.Integer<uint32_t>(0, 1000)));

        // mikkotodo how does the float distribution work?
        desc.attributes.emplace_back(
            AttributeDefIndex::TextureWear,
            random.Float(paintKitInfo->m_minFloat, paintKitInfo->m_maxFloat));
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

        desc.attributes.emplace_back(AttributeDefIndex::KillEater, 0u);

        // mikkotodo fix magic
        uint32_t scoreType = (lootListItem.type == LootListItemMusicKit) ? 1 : 0;
        desc.attributes.emplace_back(AttributeDefIndex::KillEaterScoreType, scoreType);
    }

    return true;
}

bool ItemSchema::GetItemDesc(ItemDefIndex defIndex, ItemOrigin origin, UnacknowledgedType unacknowledgedType, ItemDesc &desc) const
{
    auto itemSearch = m_itemInfo.find(defIndex);
    if (itemSearch == m_itemInfo.end())
    {
        assert(false);
        return false;
    }

    const ItemInfo &itemInfo = itemSearch->second;

    // urgh wtf is this crap
    if (itemInfo.m_isCoupon)
    {
        assert(itemInfo.m_lootListName.size());
        auto lootListSearch = m_lootLists.find(itemInfo.m_lootListName);
        if (lootListSearch == m_lootLists.end())
        {
            assert(false);
            return false;
        }

        const LootList &lootList = lootListSearch->second;
        assert(lootList.subLists.size() == 0 && lootList.items.size() == 1);
        assert(lootList.willProduceStatTrak == false && lootList.isUnusual == false);

        Random random;

        return ItemDescForLootListItem(random,
            lootList.items.front(),
            itemInfo.m_willProduceStatTrak,
            origin,
            unacknowledgedType,
            desc);
    }

    desc.inventory = InventoryUnacknowledged(unacknowledgedType);
    desc.defIndex = defIndex;
    desc.level = itemInfo.m_level;
    desc.quality = itemInfo.m_quality;
    desc.flags = 0;
    desc.origin = origin;
    desc.inUse = false;
    desc.rarity = itemInfo.m_rarity;

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

        ItemDefIndex defIndex = ToEnum<ItemDefIndex>(FromString<uint32_t>(itemKey.Name()));
        auto emplace = m_itemInfo.try_emplace(defIndex, defIndex);

        ParseItemRecursive(emplace.first->second, itemKey, prefabsKey);

        // FIXME: remove, temp slop to make sure we parse correctly
        auto &itemInfo = emplace.first->second;
        if (!itemInfo.m_isCoupon)
        {
            // FIXME: self opening purchases
            if (itemInfo.m_lootListName.size())
            {
                Platform::Print("Non coupon item associated loot list in %s!!!\n", itemInfo.m_name.c_str());
            }

            //assert(!itemInfo.m_lootListName.size());
            assert(!itemInfo.m_willProduceStatTrak);
        }
        else
        {
            assert(itemInfo.m_lootListName.size());
        }
    }
}

// i hate my life
static std::vector<std::string_view> SplitString(std::string_view input, char delimiter)
{
    size_t offset = 0;
    std::vector<std::string_view> result;

    while (true)
    {
        size_t i = input.find(delimiter, offset);
        if (i == std::string_view::npos)
        {
            result.emplace_back(input.substr(offset));
            break;
        }

        result.emplace_back(input.substr(offset, i - offset));
        offset = i + 1;
    }

    return result;
}

void ItemSchema::ParseItemRecursive(ItemInfo &info, const KeyValue &itemKey, const KeyValue *prefabsKey)
{
    std::string_view prefabString = itemKey.GetString("prefab");
    if (prefabString.size() && prefabsKey)
    {
        // might have multiple specifications in a single statement
        std::vector<std::string_view> prefabNames = SplitString(prefabString, ' ');
        for (std::string_view prefabName : prefabNames)
        {
            const KeyValue *prefabKey = prefabsKey->GetSubkey(prefabName);
            if (prefabKey)
            {
                ParseItemRecursive(info, *prefabKey, prefabsKey);
            }
            else
            {
                // not available to us mortals...
                Platform::Print("No such prefab '%s'\n", std::string{ prefabName }.c_str());
            }
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

    uint32_t minLevel = itemKey.GetNumber<uint32_t>("min_ilevel", 0);
    uint32_t maxLevel = itemKey.GetNumber<uint32_t>("max_ilevel", 0);
    if (minLevel && maxLevel)
    {
        assert(minLevel == maxLevel);
        info.m_level = minLevel;
    }

    std::string_view itemType = itemKey.GetString("item_type");
    if (itemType.size())
    {
        info.m_isCoupon = (itemType == "coupon");
    }

    std::string_view lootListName = itemKey.GetString("loot_list_name");
    if (lootListName.size())
    {
        info.m_lootListName = lootListName;
    }

    info.m_willProduceStatTrak = itemKey.GetNumber("will_produce_stattrak", false);

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
        m_attributeInfo.try_emplace(ToEnum<AttributeDefIndex>(defIndex), attributeKey);
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

        assert(paintKitInfo->m_rarity == Rarity::Common);
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
                    item.quality = Quality::Unusual;
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

static Rarity PaintedItemRarity(Rarity itemRarity, Rarity paintKitRarity)
{
    int rarityValue = (static_cast<int>(itemRarity) - 1) + static_cast<int>(paintKitRarity);
    if (rarityValue < 0)
    {
        return Rarity::Default;
    }

    Rarity rarity = static_cast<Rarity>(rarityValue);
    if (rarity > Rarity::Ancient)
    {
        if (paintKitRarity == Rarity::Immortal)
        {
            return Rarity::Immortal;
        }

        return Rarity::Ancient;
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
        assert(itemInfo->m_rarity == Rarity::Common);

        if (item.stickerKitInfo->m_rarity != Rarity::Default)
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
