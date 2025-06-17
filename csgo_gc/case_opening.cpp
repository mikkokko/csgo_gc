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
    return m_itemSchema.CreateItemFromLootListItem(m_random, *lootListItem, statTrak, ItemOriginCrate, UnacknowledgedFoundInCrate, item);
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
