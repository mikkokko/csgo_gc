#pragma once

#include "item_schema_const.h"

class ItemSchema;
class Random;

struct ItemDesc;
struct LootListItem;
struct LootList;

class CaseOpening
{
public:
    CaseOpening(const ItemSchema &itemSchema, Random &random);

    bool SelectItemFromCrate(ItemDefIndex crateDefIndex, ItemDesc &desc);

private:
    const LootListItem *SelectLootListItem(const std::vector<const LootListItem *> &items);
    Rarity RandomRarityForItems(const std::vector<const LootListItem *> &items);
    bool ShouldMakeStatTrak(const LootListItem &item, const LootList &lootList, bool containsUnusuals);

    const ItemSchema &m_itemSchema;
    Random &m_random;
};
