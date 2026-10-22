#include "stdafx.h"
#include "case_opening.h"
#include "config.h"
#include "item.h"
#include "item_schema.h"
#include "random.h"

struct RarityGroup
{
    std::span<const LootListItem *const> items;
    float weight;
};

CaseOpening::CaseOpening(const ItemSchema &itemSchema)
    : m_itemSchema{ itemSchema }
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

static bool CompareRarity(const LootListItem *a, const LootListItem *b)
{
    return a->CaseRarity() < b->CaseRarity();
}

bool CaseOpening::SelectItemFromCrate(ItemDefIndex crateDefIndex, ItemDesc &desc)
{
    const LootList *lootList = m_itemSchema.GetCrateLootList(crateDefIndex);
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

    // must be sorted for SelectLootListItem
    std::sort(lootListItems.begin(), lootListItems.end(), CompareRarity);

    const LootListItem *lootListItem = SelectLootListItem(lootListItems);
    if (!lootListItem)
    {
        assert(false);
        return false;
    }

    bool statTrak = ShouldMakeStatTrak(*lootListItem, *lootList, containsUnusuals);
    return m_itemSchema.ItemDescForLootListItem(*lootListItem, statTrak, ItemOriginCrate, UnacknowledgedFoundInCrate, desc);
}

const LootListItem *CaseOpening::SelectLootListItem(std::span<const LootListItem *const> sortedItems)
{
    std::vector<RarityGroup> groups;
    groups.reserve(sortedItems.size()); // overkill

    float totalWeight = 0;

    // group consecutive items of the same rarity
    for (auto first = sortedItems.begin(); first != sortedItems.end();)
    {
        Rarity rarity = (*first)->CaseRarity();

        auto last = first + 1;
        while (last != sortedItems.end() && (*last)->CaseRarity() == rarity)
        {
            last++;
        }

        float weight = GetConfig().GetRarityWeight(rarity);
        groups.push_back({ { first, last }, weight });
        totalWeight += weight;

        first = last;
    }

    // play the game of chance...
    float value = Random::Float(0.0f, totalWeight);
    float accum = 0.0f;

    for (const RarityGroup &group : groups)
    {
        accum += group.weight;
        if (value < accum)
        {
            return Random::Select(group.items);
        }
    }

    assert(false);
    return nullptr;
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
    if (item.quality == Quality::Unusual
        && item.itemInfo->m_defIndex >= ToEnum<ItemDefIndex>(1000u))
    {
        return false;
    }

    // roll the dice
    return (Random::Uint32(1, 10) == 1);
}
