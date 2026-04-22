#pragma once

#include "gc_const_csgo.h"
#include "item_schema.h" // rarity constants

struct RarityWeight
{
    uint32_t rarity;
    float weight;
};

class GCConfig
{
public:
    GCConfig();

    // options used by steam hook
    uint32_t AppIdOverride() const { return m_appIdOverride; }
    bool ShowCsgoGCServersOnly() const { return m_showCsgoGCServersOnly; }

    RankId CompetitiveRank() const { return m_competitiveRank; }
    int CompetitiveWins() const { return m_competitiveWins; }
    RankId WingmanRank() const { return m_wingmanRank; }
    int WingmanWins() const { return m_wingmanWins; }
    DangerZoneRankId DangerZoneRank() const { return m_dangerZoneRank; }
    int DangerZoneWins() const { return m_dangerZoneWins; }

    bool DestroyUsedItems() const { return m_destroyUsedItems; }
    bool RandomizeFloat() const { return m_randomizeFloat; }

    bool VacBanned() const { return m_vacBanned; }
    int CommendedFriendly() const { return m_commendedFriendly; }
    int CommendedTeaching() const { return m_commendedTeaching; }
    int CommendedLeader() const { return m_commendedLeader; }
    int Level() const { return m_level; }
    int Xp() const { return m_xp; }

    float GetRarityWeight(uint32_t rarity) const;

    std::vector<int> GetFriends() const { return m_friends; };

private:
    // actually default to 4465480 instead of 730, people are going to use old configs
    // and then wonder why the game doesn't work and open an issue on github otherwise
    uint32_t m_appIdOverride{ 4465480 };
    bool m_showCsgoGCServersOnly{ true };

    RankId m_competitiveRank{ RankNone };
    int m_competitiveWins{ 0 };
    RankId m_wingmanRank{ RankNone };
    int m_wingmanWins{ 0 };
    DangerZoneRankId m_dangerZoneRank{ DangerZoneRankNone };
    int m_dangerZoneWins{ 0 };

    bool m_destroyUsedItems{ true };
    bool m_randomizeFloat{ true };

    bool m_vacBanned{ false };
    int m_commendedFriendly{ 0 };
    int m_commendedTeaching{ 0 };
    int m_commendedLeader{ 0 };
    int m_level{ 0 };
    int m_xp{ 0 };

    // default to valve weights
    std::vector<RarityWeight> m_rarityWeights{
        { ItemSchema::RarityCommon, 10000000 },
        { ItemSchema::RarityUncommon, 2000000 },
        { ItemSchema::RarityRare, 400000 },
        { ItemSchema::RarityMythical, 80000 },
        { ItemSchema::RarityLegendary, 16000 },
        { ItemSchema::RarityAncient, 3200 },
        { ItemSchema::RarityUnusual, 1280 },
    };

    std::vector<int> m_friends{ 1140104601 };
};

const GCConfig &GetConfig();