#pragma once

// csgo specific gc constants

// hardcoded gscookieid
constexpr uint64_t GameServerCookieId = 0x293A206F6C6C6548;

// if an item id has the high 4 bits set, this is actually a def index
// and paint kit index in a trenchcoat instead of a valid item id!
// lowest 16 bits = def index
// next lowest 16 bits = paint kit index
constexpr uint64_t ItemIdDefaultItemMask = 0xfull << 60;

enum RankType : uint32_t
{
    RankTypeCompetitive = 6,
    RankTypeWingman = 7,
    RankTypeDangerZone = 10
};

enum RankId : uint32_t
{
    RankNone,
    RankSilver1,
    RankSilver2,
    RankSilver3,
    RankSilver4,
    RankSilverElite,
    RankSilverEliteMaster,
    RankGoldNova1,
    RankGoldNova2,
    RankGoldNova3,
    RankGoldNovaMaster,
    RankMasterGuardian1,
    RankMasterGuardian2,
    RankMasterGuardianElite,
    RankDistinguishedMasterGuardian,
    RankLegendaryEagle,
    RankLegendaryEagleMaster,
    RankSupremeMasterFirstClass,
    RankGlobalElite
};

enum DangerZoneRankId : uint32_t
{
    DangerZoneRankNone,
    DangerZoneRankRat1,
    DangerZoneRankRat2,
    DangerZoneRankHare1,
    DangerZoneRankHare2,
    DangerZoneRankScout1,
    DangerZoneRankScout2,
    DangerZoneRankScoutElite,
    DangerZoneRankFox1,
    DangerZoneRankFox2,
    DangerZoneRankFox3,
    DangerZoneRankFoxElite,
    DangerZoneRankTimberWolf,
    DangerZoneRankEmberWolf,
    DangerZoneRankWildfireWolf,
    DangerZoneRankHowlingAlpha
};

// shared object type (type_id field in CMsgSOCacheSubscribed_SubscribedType)
enum SOTypeId : uint32_t
{
    SOTypeItem = 1,
    SOTypePersonaDataPublic = 2,
    SOTypeEquipSlot = 3,
    SOTypeGameAccountClient = 7,
    SOTypeDefaultEquippedDefinitionInstanceClient = 43
};

enum ElevatedState : uint32_t
{
    ElevatedStateNo = 0,
    ElevatedStatePrime = 5
};

// struct based messages

#pragma pack(push, 1)

// k_EMsgGCUnlockCrate
struct CMsgGCUnlockCrate
{
    uint64_t key_id;
    uint64_t crate_id;
};

#pragma pack(pop)
