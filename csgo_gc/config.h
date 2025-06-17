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

    float GetRarityWeight(uint32_t rarity) const;

private:
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
};
