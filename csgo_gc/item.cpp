#include "stdafx.h"
#include "item.h"
#include "keyvalue.h"

#include "base_gcmessages.pb.h"
#include "cstrike15_gcmessages.pb.h"

Item::Item(uint32_t highId, uint32_t accountId, const KeyValue &kv, const ItemSchema &itemSchema)
    : m_highId{ highId }
{
    m_inventory = kv.GetNumber<uint32_t>("inventory");
    m_defIndex = ToEnum<ItemDefIndex>(kv.GetNumber<uint32_t>("def_index"));
    m_level = kv.GetNumber<uint32_t>("level");
    m_quality = ToEnum<Quality>(kv.GetNumber<uint32_t>("quality"));
    m_flags = kv.GetNumber<uint32_t>("flags");
    m_origin = kv.GetNumber<uint32_t>("origin");
    m_inUse = kv.GetNumber<int>("in_use") ? true : false;
    m_rarity = ToEnum<Rarity>(kv.GetNumber<uint32_t>("rarity"));

    const KeyValue *attributesKey = kv.GetSubkey("attributes");
    if (attributesKey)
    {
        m_attributes.reserve(attributesKey->SubkeyCount());

        for (const KeyValue &attributeKey : *attributesKey)
        {
            AttributeDefIndex defIndex = ToEnum<AttributeDefIndex>(FromString<uint32_t>(attributeKey.Name()));
            std::string_view value = attributeKey.String();

            // fix up stored item ids so loading an inventory file saved
            // on another steam account doesn't fuck things up
            if (defIndex == AttributeDefIndex::CasketIdLow)
            {
                m_attributes.emplace_back(defIndex, accountId);
                continue;
            }

            switch (itemSchema.GetAttributeType(defIndex))
            {
            case AttributeType::Float:
                m_attributes.emplace_back(defIndex, FromString<float>(value));
                break;

            case AttributeType::Uint32:
                m_attributes.emplace_back(defIndex, FromString<uint32_t>(value));
                break;

            case AttributeType::String:
                m_attributes.emplace_back(defIndex, std::string{ value });
                break;
            }
        }
    }

    // if no attribute name was provided, check the old custom_name field
    if (!HasAttribute(AttributeDefIndex::CustomName))
    {
        std::string_view name = kv.GetString("custom_name");
        if (name.size())
        {
            m_attributes.emplace_back(AttributeDefIndex::CustomName, std::string{ name });
        }
    }

    const KeyValue *equippedStateKey = kv.GetSubkey("equipped_state");
    if (equippedStateKey)
    {
        m_equips.reserve(equippedStateKey->SubkeyCount());

        for (const KeyValue &equippedKey : *equippedStateKey)
        {
            uint32_t equipClass = FromString<uint32_t>(equippedKey.Name());
            uint32_t equpSlot = FromString<uint32_t>(equippedKey.String());
            m_equips.emplace_back(equipClass, equpSlot);
        }
    }
}

// FIXME: take rvalue ref instead??? common pattern is to
// get the desc from schema and just create an item using it
Item::Item(uint32_t highId, const ItemDesc &desc)
    : m_highId{ highId }
{
    m_inventory = desc.inventory;
    m_defIndex = desc.defIndex;
    m_level = desc.level;
    m_quality = desc.quality;
    m_flags = desc.flags;
    m_origin = desc.origin;
    m_inUse = desc.inUse;
    m_rarity = desc.rarity;
    m_attributes = desc.attributes;
    m_equips = desc.equips;
}

void Item::ToKeyValue(KeyValue &kv) const
{
    kv.AddNumber("inventory", m_inventory);
    kv.AddNumber("def_index", FromEnum(m_defIndex));
    kv.AddNumber("level", m_level);
    kv.AddNumber("quality", FromEnum(m_quality));
    kv.AddNumber("flags", m_flags);
    kv.AddNumber("origin", m_origin);
    kv.AddNumber("in_use", m_inUse);
    kv.AddNumber("rarity", FromEnum(m_rarity));

    KeyValue &attributesKey = kv.AddSubkey("attributes");
    for (const ItemAttribute &attribute : m_attributes)
    {
        std::string name = std::to_string(FromEnum(attribute.DefIndex()));

        // illegible bullshit
        std::string value = std::visit(Bruh{
                                           [&](float v)
                                           { return std::to_string(v); },
                                           [&](uint32_t v)
                                           { return std::to_string(v); },
                                           [&](const std::string &v)
                                           { return v; },
                                       },
            attribute.Value());

        attributesKey.AddString(name, value);
    }

    KeyValue &equippedStateKey = kv.AddSubkey("equipped_state");
    for (const ItemEquip &equip : m_equips)
    {
        equippedStateKey.AddNumber(std::to_string(equip.Class()), equip.Slot());
    }
}

static void SetAttributeFloat(CSOEconItemAttribute *attribute, float value)
{
    attribute->set_value_bytes(&value, sizeof(value));
}

static void SetAttributeUint32(CSOEconItemAttribute *attribute, uint32_t value)
{
    attribute->set_value_bytes(&value, sizeof(value));
}

static void SetAttributeString(CSOEconItemAttribute *attribute, std::string_view value)
{
    CAttribute_String string;
    string.set_value(value);
    string.SerializeToString(attribute->mutable_value_bytes());
}

void Item::ToCSOEconItem(CSOEconItem &item, uint32_t accountId) const
{
    item.set_id(FullIdFor(accountId));
    item.set_account_id(accountId);
    item.set_inventory(m_inventory);
    item.set_def_index(FromEnum(m_defIndex));
    item.set_quantity(1);
    item.set_level(m_level);
    item.set_quality(FromEnum(m_quality));
    item.set_flags(m_flags);
    item.set_origin(m_origin);
    item.set_in_use(m_inUse);
    item.set_rarity(FromEnum(m_rarity));

    for (const ItemAttribute &attribute : m_attributes)
    {
        CSOEconItemAttribute *econ = item.add_attribute();
        econ->set_def_index(FromEnum(attribute.DefIndex()));

        // illegible bullshit
        std::visit(Bruh{
                       [&](float v)
                       { SetAttributeFloat(econ, v); },
                       [&](uint32_t v)
                       { SetAttributeUint32(econ, v); },
                       [&](const std::string &v)
                       { SetAttributeString(econ, v); },
                   },
            attribute.Value());
    }

    for (const ItemEquip &equip : m_equips)
    {
        CSOEconItemEquipped *econ = item.add_equipped_state();
        econ->set_new_class(equip.Class());
        econ->set_new_slot(equip.Slot());
    }
}

// mikkotodo constant enum
static uint32_t ItemWearLevel(float wearFloat)
{
    if (wearFloat < 0.07f)
    {
        // factory new
        return 0;
    }

    if (wearFloat < 0.15f)
    {
        // minimal wear
        return 1;
    }

    if (wearFloat < 0.37f)
    {
        // field tested
        return 2;
    }

    if (wearFloat < 0.45f)
    {
        // well worn
        return 3;
    }

    // battle scarred
    return 4;
}

void Item::ToEconItemPreviewDataBlock(CEconItemPreviewDataBlock &block, uint32_t accountId) const
{
    block.set_accountid(accountId);
    block.set_itemid(FullIdFor(accountId));
    block.set_defindex(FromEnum(m_defIndex));
    block.set_rarity(FromEnum(m_rarity));
    block.set_quality(FromEnum(m_quality));
    block.set_inventory(m_inventory);
    block.set_origin(m_origin);

    // not stored in CSOEconItem?
    // block.set_entindex(m_entIndex);
    // block.set_dropreason(m_dropReason);

    std::array<CEconItemPreviewDataBlock_Sticker, MaxStickers> stickers;

    for (const ItemAttribute &attribute : m_attributes)
    {
        switch (attribute.DefIndex())
        {
        case AttributeDefIndex::TexturePrefab:
            block.set_paintindex(static_cast<uint32_t>(attribute.ValueAs<float>()));
            break;

        case AttributeDefIndex::TextureSeed:
            block.set_paintseed(static_cast<uint32_t>(attribute.ValueAs<float>()));
            break;

        case AttributeDefIndex::TextureWear:
            block.set_paintwear(ItemWearLevel(attribute.ValueAs<float>()));
            break;

        case AttributeDefIndex::KillEater:
            block.set_killeatervalue(attribute.ValueAs<uint32_t>());
            break;

        case AttributeDefIndex::KillEaterScoreType:
            block.set_killeaterscoretype(attribute.ValueAs<uint32_t>());
            break;

        case AttributeDefIndex::CustomName:
            block.set_customname(attribute.ValueAs<std::string>());
            break;

        case AttributeDefIndex::MusicId:
            block.set_musicindex(attribute.ValueAs<uint32_t>());
            break;

        case AttributeDefIndex::QuestId:
            block.set_questid(attribute.ValueAs<uint32_t>());
            break;

        case AttributeDefIndex::SprayTintId:
            stickers[0].set_tint_id(attribute.ValueAs<uint32_t>());
            break;

        case AttributeDefIndex::StickerId0:
            stickers[0].set_sticker_id(attribute.ValueAs<uint32_t>());
            break;

        case AttributeDefIndex::StickerWear0:
            stickers[0].set_wear(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerScale0:
            stickers[0].set_scale(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerRotation0:
            stickers[0].set_rotation(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerId1:
            stickers[1].set_sticker_id(attribute.ValueAs<uint32_t>());
            break;

        case AttributeDefIndex::StickerWear1:
            stickers[1].set_wear(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerScale1:
            stickers[1].set_scale(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerRotation1:
            stickers[1].set_rotation(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerId2:
            stickers[2].set_sticker_id(attribute.ValueAs<uint32_t>());
            break;

        case AttributeDefIndex::StickerWear2:
            stickers[2].set_wear(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerScale2:
            stickers[2].set_scale(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerRotation2:
            stickers[2].set_rotation(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerId3:
            stickers[3].set_sticker_id(attribute.ValueAs<uint32_t>());
            break;

        case AttributeDefIndex::StickerWear3:
            stickers[3].set_wear(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerScale3:
            stickers[3].set_scale(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerRotation3:
            stickers[3].set_rotation(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerId4:
            stickers[4].set_sticker_id(attribute.ValueAs<uint32_t>());
            break;

        case AttributeDefIndex::StickerWear4:
            stickers[4].set_wear(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerScale4:
            stickers[4].set_scale(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerRotation4:
            stickers[4].set_rotation(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerId5:
            stickers[5].set_sticker_id(attribute.ValueAs<uint32_t>());
            break;

        case AttributeDefIndex::StickerWear5:
            stickers[5].set_wear(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerScale5:
            stickers[5].set_scale(attribute.ValueAs<float>());
            break;

        case AttributeDefIndex::StickerRotation5:
            stickers[5].set_rotation(attribute.ValueAs<float>());
            break;
        }
    }

    for (size_t i = 0; i < stickers.size(); i++)
    {
        const CEconItemPreviewDataBlock_Sticker &source = stickers[i];
        if (!source.has_sticker_id())
        {
            continue;
        }

        CEconItemPreviewDataBlock_Sticker *sticker = block.add_stickers();
        *sticker = source;
        sticker->set_slot(i);
    }
}

void Item::ToDesc(ItemDesc &desc) const
{
    desc.inventory = m_inventory;
    desc.defIndex = m_defIndex;
    desc.level = m_level;
    desc.quality = m_quality;
    desc.flags = m_flags;
    desc.origin = m_origin;
    desc.inUse = m_inUse;
    desc.rarity = m_rarity;
    desc.attributes = m_attributes;
    desc.equips = m_equips;
}
