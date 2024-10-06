#include "stdafx.h"
#include "config.h"
#include "keyvalue.h"

constexpr const char *ConfigFilePath = "csgo_gc/config.txt";

GCConfig::GCConfig()
{
    KeyValue config{ "config" };

    if (!config.ParseFromFile(ConfigFilePath))
    {
        return;
    }

    const KeyValue *ranks = config.GetSubkey("ranks");
    if (ranks)
    {
        m_competitiveRank = static_cast<RankId>(ranks->GetNumber<int>("competitive_rank"));
        m_competitiveWins = ranks->GetNumber<int>("competitive_wins");

        m_wingmanRank = static_cast<RankId>(ranks->GetNumber<int>("wingman_rank"));
        m_wingmanWins = ranks->GetNumber<int>("wingman_wins");

        m_dangerZoneRank = static_cast<DangerZoneRankId>(ranks->GetNumber<int>("dangerzone_rank"));
        m_dangerZoneWins = ranks->GetNumber<int>("dangerzone_wins");
    }

    m_vacBanned = config.GetNumber<int>("vac_banned") ? true : false;
    m_commendedFriendly = config.GetNumber<int>("cmd_friendly");
    m_commendedTeaching = config.GetNumber<int>("cmd_teaching");
    m_commendedLeader = config.GetNumber<int>("cmd_leader");
    m_level = config.GetNumber<int>("player_level");
    m_xp = config.GetNumber<int>("player_cur_xp");
}
