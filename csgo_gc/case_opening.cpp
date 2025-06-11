#include "stdafx.h"
#include "case_opening.h"
#include "config.h"
#include "item_schema.h"
#include "random.h"

CaseOpening::CaseOpening(const ItemSchema &itemSchema, const GCConfig &config, Random &random)
    : m_itemSchema{ itemSchema }
    , m_config{ config }
    , m_random{ random }
{
}

// returns true if there are unusuals (stattrak able)
static bool GetLootListItems(const LootList &lootList, std::vector<const LootListItem *> &items)
{
    bool unusuals = lootList.isUnusual;

    for (const LootList *other : lootList.subLists)
    {
        unusuals |= GetLootListItems(*other, items);
    }

    for (const LootListItem &item : lootList.items)
    {
        items.push_back(&item);
    }

    return unusuals;
}

static bool CompareRarity(const LootListItem *a, const LootListItem *b) { return a->CaseRarity() < b->CaseRarity(); }
static bool RarityLower(const LootListItem *a, uint32_t b) { return a->CaseRarity() < b; }
static bool RarityUpper(uint32_t a, const LootListItem *b) { return a < b->CaseRarity(); }

bool CaseOpening::SelectItemFromCrate(const CSOEconItem &crate, CSOEconItem &item)
{
    const LootList *lootList = m_itemSchema.GetCrateLootList(crate.def_index());
    if (!lootList)
    {
        assert(false);
        return false;
    }

    assert(lootList->subLists.empty() != lootList->items.empty());

    std::vector<const LootListItem *> lootListItems;
    lootListItems.reserve(32); // overkill
    bool containsUnusuals = GetLootListItems(*lootList, lootListItems);
    if (!lootListItems.size())
    {
        assert(false);
        return false;
    }

    // must be sorted for SelectLootListItem and friends
    std::sort(lootListItems.begin(), lootListItems.end(), CompareRarity);

    const LootListItem *lootListItem = SelectLootListItem(lootListItems);
    if (!lootListItem)
    {
        assert(false);
        return false;
    }

    bool statTrak = ShouldMakeStatTrak(*lootListItem, *lootList, containsUnusuals);
    return EconItemFromLootListItem(*lootListItem, item, statTrak);
}

// get a range of loot list items with a specific rarity from a vector sorted by rarity
static std::pair<size_t, size_t> FindRarityRange(const std::vector<const LootListItem *> &items, uint32_t rarity)
{
    // MUST have items and they MUST be sorted
    assert(items.size() && std::is_sorted(items.begin(), items.end(), CompareRarity));

    auto lower = std::lower_bound(items.begin(), items.end(), rarity, RarityLower);
    auto upper = std::upper_bound(lower, items.end(), rarity, RarityUpper);

    size_t begin = std::distance(items.begin(), lower);
    size_t end = std::distance(items.begin(), upper);

    return std::make_pair(begin, end);
}

const LootListItem *CaseOpening::SelectLootListItem(const std::vector<const LootListItem *> &items)
{
    uint32_t rarity = RandomRarityForItems(items);

    auto [begin, end] = FindRarityRange(items, rarity);
    if (begin == end)
    {
        assert(false);
        return nullptr;
    }

    size_t index = m_random.Integer(begin, end - 1);
    return items[index];
}

uint32_t CaseOpening::RandomRarityForItems(const std::vector<const LootListItem *> &items)
{
    // MUST have items and they MUST be sorted
    assert(items.size() && std::is_sorted(items.begin(), items.end(), CompareRarity));

    std::vector<RarityWeight> weights;
    weights.reserve(items.size()); // overkill

    float totalWeight = 0;

    // items are sorted by rarity, so iterate through the available rarities like this
    for (size_t i = 0; i < items.size(); i++)
    {
        uint32_t rarity = items[i]->CaseRarity();
        float weight = m_config.GetRarityWeight(rarity);

        weights.push_back({ rarity, weight });
        totalWeight += weight;

        // skip over any duplicate rarities
        while (i + 1 < items.size() && items[i]->CaseRarity() == items[i + 1]->CaseRarity())
        {
            i++;
        }
    }

    // play the game of chance...
    float value = m_random.Float(0.0f, totalWeight);
    float accum = 0.0f;

    for (const RarityWeight &pair : weights)
    {
        accum += pair.weight;
        if (value < accum)
        {
            return pair.rarity;
        }
    }

    assert(false);
    return 0;
}

bool CaseOpening::ShouldMakeStatTrak(const LootListItem &item, const LootList &lootList, bool containsUnusuals)
{
    if (lootList.willProduceStatTrak)
    {
        // must be stattrak
        return true;
    }

    if (!containsUnusuals)
    {
        // not a chance
        return false;
    }

    // unusual stattraks only valid below id 1000
    if (item.quality == ItemSchema::QualityUnusual
        && item.itemInfo->m_defIndex >= 1000)
    {
        return false;
    }

    // roll the dice
    return (m_random.Integer(1, 10) == 1);
}

bool CaseOpening::EconItemFromLootListItem(const LootListItem &lootListItem, CSOEconItem &item, bool statTrak)
{
    if (!m_itemSchema.CreateItem(lootListItem.itemInfo->m_defIndex, ItemOriginCrate, UnacknowledgedFoundInCrate, item))
    {
        assert(false);
        return false;
    }

    // quality override, stattrak makes it strange if it's not an unusual
    if (statTrak && lootListItem.quality != ItemSchema::QualityUnusual)
    {
        item.set_quality(ItemSchema::QualityStrange);
    }
    else
    {
        item.set_quality(lootListItem.quality);
    }

    // rarity override
    assert(lootListItem.rarity >= ItemSchema::RarityCommon && lootListItem.rarity <= ItemSchema::RarityImmortal);
    item.set_rarity(lootListItem.rarity);

    // setup type specficic attributes

    if (lootListItem.type == LootListItemSticker)
    {
        // mikkotodo anything else?
        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(ItemSchema::AttributeStickerId0);
        m_itemSchema.SetAttributeUint32(attribute, lootListItem.stickerKitInfo->m_defIndex);
    }
    else if (lootListItem.type == LootListItemSpray)
    {
        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(ItemSchema::AttributeStickerId0);
        m_itemSchema.SetAttributeUint32(attribute, lootListItem.stickerKitInfo->m_defIndex);

        // add AttributeSpraysRemaining when it's unsealed (mikkotodo how does the real gc do this)

        attribute = item.add_attribute();
        attribute->set_def_index(ItemSchema::AttributeSprayTintId);
        m_itemSchema.SetAttributeUint32(attribute, m_random.Integer<uint32_t>(ItemSchema::GraffitiTintMin, ItemSchema::GraffitiTintMax));
    }
    else if (lootListItem.type == LootListItemPatch)
    {
        // mikkotodo anything else?
        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(ItemSchema::AttributeStickerId0);
        m_itemSchema.SetAttributeUint32(attribute, lootListItem.stickerKitInfo->m_defIndex);
    }
    else if (lootListItem.type == LootListItemMusicKit)
    {
        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(ItemSchema::AttributeMusicId);
        m_itemSchema.SetAttributeUint32(attribute, lootListItem.musicDefinitionInfo->m_defIndex);
    }
    else if (lootListItem.type == LootListItemPaintable)
    {
        const PaintKitInfo *paintKitInfo = lootListItem.paintKitInfo;

        CSOEconItemAttribute *attribute = item.add_attribute();
        attribute->set_def_index(ItemSchema::AttributeTexturePrefab);
        m_itemSchema.SetAttributeUint32(attribute, paintKitInfo->m_defIndex);

        attribute = item.add_attribute();
        attribute->set_def_index(ItemSchema::AttributeTextureSeed);
        m_itemSchema.SetAttributeUint32(attribute, m_random.Integer<uint32_t>(0, 1000));

        // mikkotodo how does the float distribution work?
        attribute = item.add_attribute();
        attribute->set_def_index(ItemSchema::AttributeTextureWear);
        m_itemSchema.SetAttributeFloat(attribute, m_random.Float(paintKitInfo->m_minFloat, paintKitInfo->m_maxFloat));
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
        attribute->set_def_index(ItemSchema::AttributeKillEater);
        m_itemSchema.SetAttributeUint32(attribute, 0);

        // mikkotodo fix magic
        int scoreType = (lootListItem.type == LootListItemMusicKit) ? 1 : 0;

        attribute = item.add_attribute();
        attribute->set_def_index(ItemSchema::AttributeKillEaterScoreType);
        m_itemSchema.SetAttributeUint32(attribute, scoreType);
    }

    return true;
}
