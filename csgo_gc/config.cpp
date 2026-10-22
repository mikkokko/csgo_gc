#include "stdafx.h"
#include "config.h"
#include "keyvalue.h"

constexpr const char *ConfigFilePath = "csgo_gc/config.txt";

static GCConfig s_config;

void LoadConfig()
{
    s_config.Load();
}

const GCConfig &GetConfig()
{
    return s_config;
}

void GCConfig::Load()
{
    KeyValueRead configRead{ ConfigFilePath };
    if (!configRead.Good())
    {
        return;
    }

    const KeyValue &config = configRead.Root();

    m_logOutput = config.GetNumber("log_output", m_logOutput);

    m_appIdOverride = config.GetNumber("appid_override", m_appIdOverride);
    m_showCsgoGCServersOnly = config.GetNumber("show_csgo_gc_servers_only", m_showCsgoGCServersOnly);

    m_itemSchemaPath = config.GetString("item_schema_path", m_itemSchemaPath);

    m_inventoryEditorOrigin = config.GetString("inventory_editor_origin", m_inventoryEditorOrigin);

    const KeyValue *ranks = config.GetSubkey("ranks");
    if (ranks)
    {
        m_competitiveRank = ranks->GetNumber("competitive_rank", m_competitiveRank);
        m_competitiveWins = ranks->GetNumber("competitive_wins", m_competitiveWins);

        m_wingmanRank = ranks->GetNumber("wingman_rank", m_wingmanRank);
        m_wingmanWins = ranks->GetNumber("wingman_wins", m_wingmanWins);

        m_dangerZoneRank = ranks->GetNumber("dangerzone_rank", m_dangerZoneRank);
        m_dangerZoneWins = ranks->GetNumber("dangerzone_wins", m_dangerZoneWins);
    }

    m_destroyUsedItems = config.GetNumber("destroy_used_items", m_destroyUsedItems);

    const KeyValue *rarityWeights = config.GetSubkey("rarity_weights");
    if (rarityWeights)
    {
        m_rarityWeights.clear();
        m_rarityWeights.reserve(rarityWeights->Subkeys().size());

        for (const KeyValue &subkey : rarityWeights->Subkeys())
        {
            RarityWeight weight;
            weight.rarity = ToEnum<Rarity>(FromString<uint32_t>(subkey.Name()));
            weight.weight = FromString<float>(subkey.String());
            m_rarityWeights.push_back(weight);
        }
    }

    m_vacBanned = config.GetNumber("vac_banned", m_vacBanned);
    m_commendedFriendly = config.GetNumber("cmd_friendly", m_commendedFriendly);
    m_commendedTeaching = config.GetNumber("cmd_teaching", m_commendedTeaching);
    m_commendedLeader = config.GetNumber("cmd_leader", m_commendedLeader);
    m_level = config.GetNumber("player_level", m_level);
    m_xp = config.GetNumber("player_cur_xp", m_xp);

    m_country = config.GetString("country", m_country);
    m_currency = config.GetNumber("currency", m_currency);
}

float GCConfig::GetRarityWeight(Rarity rarity) const
{
    for (const RarityWeight &weight : m_rarityWeights)
    {
        if (weight.rarity == rarity)
        {
            return weight.weight;
        }
    }

    return 0;
}
