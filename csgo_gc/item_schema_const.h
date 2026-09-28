// these could be parsed from the item schema but reduce code complexity by hardcoding them
// FIXME: update code so we can use the automatically generated one!!!
#pragma once

enum class Rarity : uint32_t
{
    Default = 0,
    Common = 1,
    Uncommon = 2,
    Rare = 3,
    Mythical = 4,
    Legendary = 5,
    Ancient = 6,
    Immortal = 7,

    Unusual = 99
};

enum class Quality : uint32_t
{
    Normal = 0,
    Genuine = 1,
    Vintage = 2,
    Unusual = 3,
    Unique = 4,
    Community = 5,
    Developer = 6,
    Selfmade = 7,
    Customized = 8,
    Strange = 9,
    Completed = 10,
    Haunted = 11,
    Tournament = 12
};

enum class GraffitiTint : uint32_t
{
    Min = 1,
    Max = 19
};

enum class ItemDefIndex : uint32_t
{
    Casket = 1201,
    Sticker = 1209,
    MusicKit = 1314,
    Spray = 1348,
    SprayPaint = 1349,
    Patch = 4609
};

enum class AttributeDefIndex : uint32_t
{
    TexturePrefab = 6,
    TextureSeed = 7,
    TextureWear = 8,
    KillEater = 80,
    KillEaterScoreType = 81,

    CustomName = 111,

    // ugh
    StickerId0 = 113,
    StickerWear0 = 114,
    StickerScale0 = 115,
    StickerRotation0 = 116,
    StickerId1 = 117,
    StickerWear1 = 118,
    StickerScale1 = 119,
    StickerRotation1 = 120,
    StickerId2 = 121,
    StickerWear2 = 122,
    StickerScale2 = 123,
    StickerRotation2 = 124,
    StickerId3 = 125,
    StickerWear3 = 126,
    StickerScale3 = 127,
    StickerRotation3 = 128,
    StickerId4 = 129,
    StickerWear4 = 130,
    StickerScale4 = 131,
    StickerRotation4 = 132,
    StickerId5 = 133,
    StickerWear5 = 134,
    StickerScale5 = 135,
    StickerRotation5 = 136,

    MusicId = 166,
    QuestId = 168,

    SpraysRemaining = 232,
    SprayTintId = 233,

    CasketItemsCount = 270,
    CasketModificationDate = 271,
    CasketIdLow = 272,
    CasketIdHigh = 273,
};

// not really item schema!!! it does have these defs, but out of date
// better location would be gc_const_csgo.h
enum LoadoutSlot : uint32_t
{
    LoadoutSlotGraffiti = 56
};
