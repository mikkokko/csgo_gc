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
    GCConfig();

    // options used by platform layer (bruh)
    LogOutput GetLogOutput() const { return m_logOutput; }

    // options used by steam hook
    uint32_t AppIdOverride() const { return m_appIdOverride; }
    bool ShowCsgoGCServersOnly() const { return m_showCsgoGCServersOnly; }

    const std::string &InventoryEditorOrigin() const { return m_inventoryEditorOrigin; }

    RankId CompetitiveRank() const { return m_competitiveRank; }
    int CompetitiveWins() const { return m_competitiveWins; }
    RankId WingmanRank() const { return m_wingmanRank; }
    int WingmanWins() const { return m_wingmanWins; }
    DangerZoneRankId DangerZoneRank() const { return m_dangerZoneRank; }
    int DangerZoneWins() const { return m_dangerZoneWins; }

    bool DestroyUsedItems() const { return m_destroyUsedItems; }

    bool VacBanned() const { return m_vacBanned; }
    int CommendedFriendly() const { return m_commendedFriendly; }
    int CommendedTeaching() const { return m_commendedTeaching; }
    int CommendedLeader() const { return m_commendedLeader; }
    int Level() const { return m_level; }
    int Xp() const { return m_xp; }

    const std::string &Country() const { return m_country; }
    const uint32_t Currency() const { return m_currency; }

    float GetRarityWeight(Rarity rarity) const;

private:
    LogOutput m_logOutput{ LogOutputConsole };

    // actually default to 4465480 instead of 730, people are going to use old configs
    // and then wonder why the game doesn't work and open an issue on github otherwise
    uint32_t m_appIdOverride{ 4465480 };
    bool m_showCsgoGCServersOnly{ true };

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

const GCConfig &GetConfig();
