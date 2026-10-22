#pragma once

#include "item_schema_const.h"

class ItemSchema;

struct ItemDesc;
struct LootListItem;
struct LootList;

class CaseOpening
{
public:
    CaseOpening(const ItemSchema &itemSchema);

    bool SelectItemFromCrate(ItemDefIndex crateDefIndex, ItemDesc &desc);

private:
    const LootListItem *SelectLootListItem(std::span<const LootListItem *const> sortedItems);
    bool ShouldMakeStatTrak(const LootListItem &item, const LootList &lootList, bool containsUnusuals);

    const ItemSchema &m_itemSchema;
};
