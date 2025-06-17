#pragma once

class GCConfig;
class ItemSchema;
class Random;

struct LootListItem;
struct LootList;

class CaseOpening
{
public:
    CaseOpening(const ItemSchema &itemSchema, const GCConfig &config, Random &random);

    bool SelectItemFromCrate(const CSOEconItem &crate, CSOEconItem &item);

private:
    const LootListItem *SelectLootListItem(const std::vector<const LootListItem *> &items);
    uint32_t RandomRarityForItems(const std::vector<const LootListItem *> &items);
    bool ShouldMakeStatTrak(const LootListItem &item, const LootList &lootList, bool containsUnusuals);

    const ItemSchema &m_itemSchema;
    const GCConfig &m_config;
    Random &m_random;
};
