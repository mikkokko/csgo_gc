#pragma once

#include "gc_const_csgo.h"

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

    bool VacBanned() const { return m_vacBanned; }
    int CommendedFriendly() const { return m_commendedFriendly; }
    int CommendedTeaching() const { return m_commendedTeaching; }
    int CommendedLeader() const { return m_commendedLeader; }
    int Level() const { return m_level; }
    int Xp() const { return m_xp; }

private:
    RankId m_competitiveRank{ RankNone };
    int m_competitiveWins{ 0 };
    RankId m_wingmanRank{ RankNone };
    int m_wingmanWins{ 0 };
    DangerZoneRankId m_dangerZoneRank{ DangerZoneRankNone };
    int m_dangerZoneWins{ 0 };

    bool m_vacBanned{ false };
    int m_commendedFriendly{ 0 };
    int m_commendedTeaching{ 0 };
    int m_commendedLeader{ 0 };
    int m_level{ 0 };
    int m_xp{ 0 };
};
