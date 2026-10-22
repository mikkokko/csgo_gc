#pragma once

#include "gc_const_csgo.h"
#include "item_schema_const.h" // rarity constants

struct RarityWeight
{
    Rarity rarity;
    float weight;
};

// for Platform::Print calls
enum LogOutput
{
    LogOutputNone, // don't output anything
    LogOutputConsole, // game console
    LogOutputFile // game console and gc_log.txt
};

class GCConfig
{
public:
    void Load();

    float GetRarityWeight(Rarity rarity) const;

    LogOutput m_logOutput{ LogOutputConsole };

    // actually default to 4465480 instead of 730, people are going to use old configs
    // and then wonder why the game doesn't work and open an issue on github otherwise
    uint32_t m_appIdOverride{ 4465480 };
    bool m_showCsgoGCServersOnly{ true };

    std::string m_itemSchemaPath{ "csgo/scripts/items/items_game.txt" };

    std::string m_inventoryEditorOrigin;

    RankId m_competitiveRank{ RankNone };
    int m_competitiveWins{ 0 };
    RankId m_wingmanRank{ RankNone };
    int m_wingmanWins{ 0 };
    DangerZoneRankId m_dangerZoneRank{ DangerZoneRankNone };
    int m_dangerZoneWins{ 0 };

    bool m_destroyUsedItems{ true };

    bool m_vacBanned{ false };
    int m_commendedFriendly{ 0 };
    int m_commendedTeaching{ 0 };
    int m_commendedLeader{ 0 };
    int m_level{ 0 };
    int m_xp{ 0 };

    std::string m_country{ "FI" };
    int m_currency{ 2 };

private:
    // default to valve weights
    std::vector<RarityWeight> m_rarityWeights{
        { Rarity::Common, 10000000 },
        { Rarity::Uncommon, 2000000 },
        { Rarity::Rare, 400000 },
        { Rarity::Mythical, 80000 },
        { Rarity::Legendary, 16000 },
        { Rarity::Ancient, 3200 },
        { Rarity::Unusual, 1280 },
    };
};

void LoadConfig();
const GCConfig &GetConfig();
