#include "stdafx.h"
#include "inventory.h"
#include "case_opening.h"
#include "config.h"
#include "gc_const.h"
#include "keyvalue.h"
#include "random.h"

constexpr const char *InventoryFilePath = "csgo_gc/inventory.txt";

// mikkotodo actual versioning
constexpr uint64_t InventoryVersion = 7523377975160828514;

// mix the account id into item ids to avoid collisions in multiplayer games
inline uint64_t ComposeItemId(uint32_t accountId, uint32_t highItemId)
{
    uint64_t low = accountId;
    uint64_t high = highItemId;
    return low | (high << 32);
}

inline uint32_t HighItemId(uint64_t itemId)
{
    return (itemId >> 32);
}

// helper, see ItemIdDefaultItemMask for more information
inline bool IsDefaultItemId(uint64_t itemId, uint32_t &defIndex, uint32_t &paintKitIndex)
{
    if ((itemId & ItemIdDefaultItemMask) == ItemIdDefaultItemMask)
    {
        defIndex = itemId & 0xffff;
        paintKitIndex = (itemId >> 16) & 0xffff;
        return true;
    }

    return false;
}

Inventory::Inventory(uint64_t steamId, const GCConfig &config)
    : m_steamId{ steamId }
    , m_config{ config }
{
    ReadFromFile();
}

Inventory::~Inventory()
{
    WriteToFile();
}

void Inventory::AddToMultipleObjects(CMsgSOMultipleObjects &message, SOTypeId type, const google::protobuf::MessageLite &object)
{
    if (!message.has_version())
    {
        assert(!message.has_owner_soid());
        message.set_version(InventoryVersion);
        message.mutable_owner_soid()->set_type(SoIdTypeSteamId);
        message.mutable_owner_soid()->set_id(m_steamId);
    }
    else
    {
        assert(message.has_owner_soid());
    }

    CMsgSOMultipleObjects_SingleObject *single = message.add_objects_modified();
    single->set_type_id(type);
    single->set_object_data(object.SerializeAsString());
}

void Inventory::ToSingleObject(CMsgSOSingleObject &message, SOTypeId type, const google::protobuf::MessageLite &object)
{
    assert(!message.has_owner_soid());
    assert(!message.has_version());
    assert(!message.has_type_id());
    assert(!message.has_object_data());

    message.set_version(InventoryVersion);
    message.mutable_owner_soid()->set_type(SoIdTypeSteamId);
    message.mutable_owner_soid()->set_id(m_steamId);

    message.set_type_id(type);
    message.set_object_data(object.SerializeAsString());
}

uint32_t Inventory::AccountId() const
{
    return m_steamId & 0xffffffff;
}

CSOEconItem &Inventory::AllocateItem(uint32_t highItemId)
{
    // Players fuck up their inventory files constantly and end up with item id collisions...
    // This doesn't return until the item id is unique for this session, try with the provided
    // item id first, if it's invalid or already in use increment it

    if (!highItemId)
    {
        m_lastHighItemId++;
        highItemId = m_lastHighItemId;
    }

    for (;; highItemId++)
    {
        uint64_t itemId = ComposeItemId(AccountId(), highItemId);
        if ((itemId & ItemIdDefaultItemMask) == ItemIdDefaultItemMask)
        {
            // would be interpreted as a default item (it's not)
            assert(false);
            // shit error handling
            continue;
        }

        auto [it, inserted] = m_items.try_emplace(itemId);
        if (!inserted)
        {
            // item id collision
            assert(false);
            continue;
        }

        if (highItemId > m_lastHighItemId)
        {
            m_lastHighItemId = highItemId;
        }

        // ok
        CSOEconItem &item = it->second;

        item.set_id(itemId);
        item.set_account_id(AccountId());

        return item;
    }
}

CSOEconItem &Inventory::CreateItem(const CSOEconItem &copyFrom)
{
    CSOEconItem &item = AllocateItem(0);

    // shitty but what can you do
    uint64_t itemId = item.id();
    uint32_t accountId = item.account_id();

    item = copyFrom;

    item.set_id(itemId);
    item.set_account_id(accountId);

    return item;
}

CSOEconItem &Inventory::CreateItem(uint32_t defIndex, ItemOrigin origin, UnacknowledgedType unacknowledgedType)
{
    CSOEconItem &item = AllocateItem(0);
    m_itemSchema.CreateItem(defIndex, origin, unacknowledgedType, item);
    return item;
}

void Inventory::ReadFromFile()
{
    KeyValue inventoryKey{ "inventory" };
    if (!inventoryKey.ParseFromFile(InventoryFilePath))
    {
        return;
    }

    const KeyValue *itemsKey = inventoryKey.GetSubkey("items");
    if (itemsKey)
    {
        m_items.reserve(itemsKey->SubkeyCount());

        for (const KeyValue &itemKey : *itemsKey)
        {
            uint32_t highItemId = FromString<uint32_t>(itemKey.Name());
            CSOEconItem &item = AllocateItem(highItemId);
            ReadItem(itemKey, item);
        }
    }

    const KeyValue *defaultEquipsKey = inventoryKey.GetSubkey("default_equips");
    if (defaultEquipsKey)
    {
        m_defaultEquips.reserve(defaultEquipsKey->SubkeyCount());

        for (const KeyValue &defaultEquipKey : *defaultEquipsKey)
        {
            CSOEconDefaultEquippedDefinitionInstanceClient &defaultEquip = m_defaultEquips.emplace_back();
            defaultEquip.set_account_id(AccountId());
            defaultEquip.set_item_definition(FromString<uint32_t>(defaultEquipKey.Name()));
            defaultEquip.set_class_id(defaultEquipKey.GetNumber<uint32_t>("class_id"));
            defaultEquip.set_slot_id(defaultEquipKey.GetNumber<uint32_t>("slot_id"));
        }
    }
}

void Inventory::ReadItem(const KeyValue &itemKey, CSOEconItem &item) const
{
    // id and account_id were set by CreateItem
    item.set_inventory(itemKey.GetNumber<uint32_t>("inventory"));
    item.set_def_index(itemKey.GetNumber<uint32_t>("def_index"));
    //item.set_quantity(itemKey.GetNumber<uint32_t>("quantity"));
    item.set_quantity(1);
    item.set_level(itemKey.GetNumber<uint32_t>("level"));
    item.set_quality(itemKey.GetNumber<uint32_t>("quality"));
    item.set_flags(itemKey.GetNumber<uint32_t>("flags"));
    item.set_origin(itemKey.GetNumber<uint32_t>("origin"));

    std::string_view name = itemKey.GetString("custom_name");
    if (name.size())
    {
        item.set_custom_name(std::string{ name });
    }

    //std::string_view desc = itemKey.GetString("custom_desc");
    //if (desc.size())
    //{
    //    item.set_custom_desc(std::string{ desc });
    //}

    item.set_in_use(itemKey.GetNumber<int>("in_use"));
    //item.set_style(itemKey.GetNumber<uint32_t>("style"));
    //item.set_original_id(itemKey.GetNumber<uint64_t>("original_id"));
    item.set_rarity(itemKey.GetNumber<uint32_t>("rarity"));

    const KeyValue *attributesKey = itemKey.GetSubkey("attributes");
    if (attributesKey)
    {
        for (const KeyValue &attributeKey : *attributesKey)
        {
            CSOEconItemAttribute *attribute = item.add_attribute();

            uint32_t defIndex = FromString<uint32_t>(attributeKey.Name());
            attribute->set_def_index(defIndex);
            m_itemSchema.SetAttributeString(attribute, attributeKey.String());
        }
    }

    const KeyValue *equippedStateKey = itemKey.GetSubkey("equipped_state");
    if (equippedStateKey)
    {
        for (const KeyValue &equippedKey : *equippedStateKey)
        {
            CSOEconItemEquipped *equipped = item.add_equipped_state();
            equipped->set_new_class(FromString<uint32_t>(equippedKey.Name()));
            equipped->set_new_slot(FromString<uint32_t>(equippedKey.String()));
        }
    }
}

void Inventory::WriteToFile() const
{
    KeyValue inventoryKey{ "inventory" };

    {
        KeyValue &itemsKey = inventoryKey.AddSubkey("items");

        for (const auto &pair : m_items)
        {
            const CSOEconItem &item = pair.second;
            KeyValue &itemKey = itemsKey.AddSubkey(std::to_string(HighItemId(item.id())));
            WriteItem(itemKey, item);
        }
    }

    {
        KeyValue &defaultEquipsKey = inventoryKey.AddSubkey("default_equips");

        for (const CSOEconDefaultEquippedDefinitionInstanceClient &defaultEquip : m_defaultEquips)
        {
            KeyValue &defaultEquipKey = defaultEquipsKey.AddSubkey(std::to_string(defaultEquip.item_definition()));
            defaultEquipKey.AddNumber("class_id", defaultEquip.class_id());
            defaultEquipKey.AddNumber("slot_id", defaultEquip.slot_id());
        }
    }

    inventoryKey.WriteToFile(InventoryFilePath);
}

void Inventory::WriteItem(KeyValue &itemKey, const CSOEconItem &item) const
{
    itemKey.AddNumber("inventory", item.inventory());
    itemKey.AddNumber("def_index", item.def_index());
    //itemKey.AddNumber("quantity", item.quantity());
    itemKey.AddNumber("level", item.level());
    itemKey.AddNumber("quality", item.quality());
    itemKey.AddNumber("flags", item.flags());
    itemKey.AddNumber("origin", item.origin());

    itemKey.AddString("custom_name", item.custom_name());
    //itemKey.AddString("custom_desc", item.custom_desc());

    itemKey.AddNumber("in_use", item.in_use());
    //itemKey.AddNumber("style", item.style());
    //itemKey.AddNumber("original_id", item.original_id());
    itemKey.AddNumber("rarity", item.rarity());

    KeyValue &attributesKey = itemKey.AddSubkey("attributes");
    for (const CSOEconItemAttribute &attribute : item.attribute())
    {
        std::string name = std::to_string(attribute.def_index());
        std::string value = m_itemSchema.AttributeString(&attribute);
        attributesKey.AddString(name, value);
    }

    KeyValue &equippedStateKey = itemKey.AddSubkey("equipped_state");
    for (const CSOEconItemEquipped &equip : item.equipped_state())
    {
        equippedStateKey.AddNumber(std::to_string(equip.new_class()), equip.new_slot());
    }
}

void Inventory::BuildCacheSubscription(CMsgSOCacheSubscribed &message, int level, bool server)
{
    message.set_version(InventoryVersion);
    message.mutable_owner_soid()->set_type(SoIdTypeSteamId);
    message.mutable_owner_soid()->set_id(m_steamId);

    {
        CMsgSOCacheSubscribed_SubscribedType *object = message.add_objects();
        object->set_type_id(SOTypeItem);

        for (const auto &pair : m_items)
        {
            object->add_object_data(pair.second.SerializeAsString());
        }
    }

    {
        CSOPersonaDataPublic personaData;
        personaData.set_player_level(level);
        personaData.set_elevated_state(true);

        CMsgSOCacheSubscribed_SubscribedType *object = message.add_objects();
        object->set_type_id(SOTypePersonaDataPublic);
        object->add_object_data(personaData.SerializeAsString());
    }

    if (!server)
    {
        CSOEconGameAccountClient accountClient;
        accountClient.set_additional_backpack_slots(0);
        accountClient.set_bonus_xp_timestamp_refresh(static_cast<uint32_t>(time(nullptr)));
        accountClient.set_bonus_xp_usedflags(16); // caught cheater lobbies, overwatch bonus etc
        accountClient.set_elevated_state(ElevatedStatePrime);
        accountClient.set_elevated_timestamp(ElevatedStatePrime); // is this actually 5????

        CMsgSOCacheSubscribed_SubscribedType *object = message.add_objects();
        object->set_type_id(SOTypeGameAccountClient);
        object->add_object_data(accountClient.SerializeAsString());
    }

    {
        CMsgSOCacheSubscribed_SubscribedType *object = message.add_objects();
        object->set_type_id(SOTypeDefaultEquippedDefinitionInstanceClient);

        for (const CSOEconDefaultEquippedDefinitionInstanceClient &defaultEquip : m_defaultEquips)
        {
            object->add_object_data(defaultEquip.SerializeAsString());
        }
    }
}

// mikkotodo move
constexpr uint32_t SlotUneqip = 0xffff;
constexpr uint64_t ItemIdInvalid = 0;

// yes this function is inefficent!!! but i think that makes it more clear
// also i think this is the way valve gc does it???? can't remember
bool Inventory::EquipItem(uint64_t itemId, uint32_t classId, uint32_t slotId, CMsgSOMultipleObjects &update)
{
    if (slotId == SlotUneqip)
    {
        // unequipping a specific item from all slots
        return UnequipItem(itemId, update);
    }

    // mikkotodo cleanup, old junk
    assert(itemId);
    assert(itemId != UINT64_MAX); // probably an old csgo thing

    if (itemId == ItemIdInvalid)
    {
        // unequip from this slot, itemid not provided so nothing gets equipped
        UnequipItem(classId, slotId, update);
        return true;
    }

    uint32_t defIndex, paintKitIndex;
    if (IsDefaultItemId(itemId, defIndex, paintKitIndex))
    {
        // if an item is equipped in this slot, unequip it first
        UnequipItem(classId, slotId, update);

        Platform::Print("EquipItem def %u class %d slot %d\n", defIndex, classId, slotId);

        CSOEconDefaultEquippedDefinitionInstanceClient &defaultEquip = m_defaultEquips.emplace_back();
        defaultEquip.set_account_id(AccountId());
        defaultEquip.set_item_definition(defIndex);
        defaultEquip.set_class_id(classId);
        defaultEquip.set_slot_id(slotId);

        AddToMultipleObjects(update, defaultEquip);

        return true;
    }
    else
    {
        auto it = m_items.find(itemId);
        if (it == m_items.end())
        {
            Platform::Print("EquipItem: no such item %llu!!!!\n", itemId);
            return false; // didn't modify anything
        }

        // if an item is equipped in this slot, unequip it first
        UnequipItem(classId, slotId, update);

        Platform::Print("EquipItem %llu class %d slot %d\n", itemId, classId,
            slotId);

        CSOEconItem &item = it->second;

        CSOEconItemEquipped *equippedState = item.add_equipped_state();
        equippedState->set_new_class(classId);
        equippedState->set_new_slot(slotId);

        AddToMultipleObjects(update, item);

        return true;
    }
}

bool Inventory::UseItem(uint64_t itemId,
    CMsgSOSingleObject &destroy,
    CMsgSOMultipleObjects &updateMultiple,
    CMsgGCItemCustomizationNotification &notification)
{
    auto it = m_items.find(itemId);
    if (it == m_items.end())
    {
        assert(false);
        return false;
    }

    if (it->second.def_index() != ItemSchema::ItemSpray)
    {
        assert(false);
        return false;
    }

    // create an unsealed spray based on the sealed one
    CSOEconItem &unsealed = CreateItem(it->second);
    unsealed.set_def_index(ItemSchema::ItemSprayPaint);

    // remove the sealed spray from our inventory
    DestroyItem(it, destroy);

    // equip the new spray, this will also unequip the old one if we had one
    EquipItem(unsealed.id(), 0, ItemSchema::LoadoutSlotGraffiti, updateMultiple);

    // remove this to have unlimited sprays
    CSOEconItemAttribute *attribute = unsealed.add_attribute();
    attribute->set_def_index(ItemSchema::AttributeSpraysRemaining);
    m_itemSchema.SetAttributeUint32(attribute, 50);

    // set notification
    notification.add_item_id(unsealed.id());
    notification.set_request(k_EGCItemCustomizationNotification_GraffitiUnseal);

    return true;
}

bool Inventory::UnlockCrate(uint64_t crateId,
    uint64_t keyId,
    CMsgSOSingleObject &destroyCrate,
    CMsgSOSingleObject &destroyKey,
    CMsgSOSingleObject &newItem,
    CMsgGCItemCustomizationNotification &notification)
{
    auto crate = m_items.find(crateId);
    if (crate == m_items.end())
    {
        assert(false);
        return false;
    }

    // CASE OPENING
    CaseOpening caseOpening{ m_itemSchema, m_config, m_random };

    CSOEconItem temp;
    if (!caseOpening.SelectItemFromCrate(crate->second, temp))
    {
        assert(false);
        return false;
    }

    CSOEconItem &item = CreateItem(temp);

    ToSingleObject(newItem, item);

    // set notification
    notification.add_item_id(item.id());
    notification.set_request(k_EGCItemCustomizationNotification_UnlockCrate);

    // remove the crate
    if (m_config.DestroyUsedItems())
    {
        DestroyItem(crate, destroyCrate);

        // remove the key if one was used (yes, we don't validate keys...)
        auto key = m_items.find(keyId);
        if (key != m_items.end())
        {
            DestroyItem(key, destroyKey);
        }
    }

    return true;
}

// mikkotodo constant enum
static int ItemWearLevel(float wearFloat)
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

void Inventory::ItemToPreviewDataBlock(const CSOEconItem &item, CEconItemPreviewDataBlock &block)
{
    block.set_accountid(item.account_id());
    block.set_itemid(item.id());
    block.set_defindex(item.def_index());
    block.set_rarity(item.rarity());
    block.set_quality(item.quality());
    block.set_customname(item.custom_name());
    block.set_inventory(item.inventory());
    block.set_origin(item.origin());

    // not stored in CSOEconItem?
    //block.set_entindex(item.entindex());
    //block.set_dropreason(item.dropreason());

    std::array<CEconItemPreviewDataBlock_Sticker, MaxStickers> stickers;

    for (const CSOEconItemAttribute &attribute : item.attribute())
    {
        uint32_t defIndex = attribute.def_index();
        switch (defIndex)
        {
        case ItemSchema::AttributeTexturePrefab:
            block.set_paintindex(m_itemSchema.AttributeUint32(&attribute));
            break;

        case ItemSchema::AttributeTextureSeed:
            block.set_paintseed(m_itemSchema.AttributeUint32(&attribute));
            break;

        case ItemSchema::AttributeTextureWear:
        {
            int wearLevel = ItemWearLevel(m_itemSchema.AttributeFloat(&attribute));
            block.set_paintwear(wearLevel);
            break;
        }

        case ItemSchema::AttributeKillEater:
            block.set_killeatervalue(m_itemSchema.AttributeUint32(&attribute));
            break;

        case ItemSchema::AttributeKillEaterScoreType:
            block.set_killeaterscoretype(m_itemSchema.AttributeUint32(&attribute));
            break;

        case ItemSchema::AttributeMusicId:
            block.set_musicindex(m_itemSchema.AttributeUint32(&attribute));
            break;

        case ItemSchema::AttributeQuestId:
            block.set_questid(m_itemSchema.AttributeUint32(&attribute));
            break;

        case ItemSchema::AttributeSprayTintId:
            stickers[0].set_tint_id(m_itemSchema.AttributeUint32(&attribute));
            break;

        case ItemSchema::AttributeStickerId0:
            stickers[0].set_sticker_id(m_itemSchema.AttributeUint32(&attribute));
            break;

        case ItemSchema::AttributeStickerWear0:
            stickers[0].set_wear(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerScale0:
            stickers[0].set_scale(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerRotation0:
            stickers[0].set_rotation(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerId1:
            stickers[1].set_sticker_id(m_itemSchema.AttributeUint32(&attribute));
            break;

        case ItemSchema::AttributeStickerWear1:
            stickers[1].set_wear(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerScale1:
            stickers[1].set_scale(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerRotation1:
            stickers[1].set_rotation(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerId2:
            stickers[2].set_sticker_id(m_itemSchema.AttributeUint32(&attribute));
            break;

        case ItemSchema::AttributeStickerWear2:
            stickers[2].set_wear(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerScale2:
            stickers[2].set_scale(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerRotation2:
            stickers[2].set_rotation(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerId3:
            stickers[3].set_sticker_id(m_itemSchema.AttributeUint32(&attribute));
            break;

        case ItemSchema::AttributeStickerWear3:
            stickers[3].set_wear(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerScale3:
            stickers[3].set_scale(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerRotation3:
            stickers[3].set_rotation(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerId4:
            stickers[4].set_sticker_id(m_itemSchema.AttributeUint32(&attribute));
            break;

        case ItemSchema::AttributeStickerWear4:
            stickers[4].set_wear(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerScale4:
            stickers[4].set_scale(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerRotation4:
            stickers[4].set_rotation(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerId5:
            stickers[5].set_sticker_id(m_itemSchema.AttributeUint32(&attribute));
            break;

        case ItemSchema::AttributeStickerWear5:
            stickers[5].set_wear(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerScale5:
            stickers[5].set_scale(m_itemSchema.AttributeFloat(&attribute));
            break;

        case ItemSchema::AttributeStickerRotation5:
            stickers[5].set_rotation(m_itemSchema.AttributeFloat(&attribute));
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

bool Inventory::SetItemPositions(
    const CMsgSetItemPositions &message,
    std::vector<CMsgItemAcknowledged> &acknowledgements,
    CMsgSOMultipleObjects &update)
{
    for (const CMsgSetItemPositions_ItemPosition &position : message.item_positions())
    {
        auto it = m_items.find(position.item_id());
        if (it == m_items.end())
        {
            assert(false);
            return false;
        }

        CSOEconItem &item = it->second;

        Platform::Print("SetItemPositions: %llu --> %u\n", position.item_id(), position.position());

        CMsgItemAcknowledged &acknowledgement = acknowledgements.emplace_back();
        ItemToPreviewDataBlock(item, *acknowledgement.mutable_iteminfo());

        item.set_inventory(position.position());

        AddToMultipleObjects(update, item);
    }

    return true;
}

bool Inventory::ApplySticker(const CMsgApplySticker &message,
    CMsgSOSingleObject &update,
    CMsgSOSingleObject &destroy,
    CMsgGCItemCustomizationNotification &notification)
{
    assert(message.has_sticker_item_id());
    assert(message.has_sticker_slot());
    assert(!message.has_sticker_wear());

    auto sticker = m_items.find(message.sticker_item_id());
    if (sticker == m_items.end())
    {
        assert(false);
        return false;
    }

    CSOEconItem *item = nullptr;

    if (message.baseitem_defidx())
    {
        item = &CreateItem(message.baseitem_defidx(), ItemOriginBaseItem, UnacknowledgedInvalid);
    }
    else
    {
        auto it = m_items.find(message.item_item_id());
        if (it == m_items.end())
        {
            assert(false);
            return false;
        }

        item = &it->second;
    }

    assert(item);

    // get the sticker kit def index
    uint32_t stickerKit = 0;

    for (const CSOEconItemAttribute &attribute : sticker->second.attribute())
    {
        if (attribute.def_index() == ItemSchema::AttributeStickerId0)
        {
            stickerKit = m_itemSchema.AttributeUint32(&attribute);
            break;
        }
    }

    if (!stickerKit)
    {
        assert(false);
        return false;
    }

    // mikkotodo lookup table instead of this crap...
    uint32_t attributeStickerId = ItemSchema::AttributeStickerId0 + (message.sticker_slot() * 4);
    uint32_t attributeStickerWear = ItemSchema::AttributeStickerWear0 + (message.sticker_slot() * 4);

    // add the sticker id attribute
    CSOEconItemAttribute *attribute = item->add_attribute();
    attribute->set_def_index(attributeStickerId);
    m_itemSchema.SetAttributeUint32(attribute, stickerKit);

    // add the sticker wear attribute if this is not a patch (mikkotodo revisit...)
    if (sticker->second.def_index() != ItemSchema::ItemPatch)
    {
        attribute = item->add_attribute();
        attribute->set_def_index(attributeStickerWear);
        m_itemSchema.SetAttributeFloat(attribute, 0);
    }

    ToSingleObject(update, *item);

    // remove the sticker
    if (m_config.DestroyUsedItems())
    {
        DestroyItem(sticker, destroy);
    }

    // notification, if any
    notification.add_item_id(item->id());
    notification.set_request(k_EGCItemCustomizationNotification_ApplySticker);

    return true;
}

static void RemoveStickerAttributes(CSOEconItem &item, uint32_t slot)
{
    // mikkotodo lookup table instead of this crap...
    // mikkotodo rest of attribs???
    uint32_t attributeStickerId = ItemSchema::AttributeStickerId0 + (slot * 4);
    uint32_t attributeStickerWear = ItemSchema::AttributeStickerWear0 + (slot * 4);

    for (auto attrib = item.mutable_attribute()->begin(); attrib != item.mutable_attribute()->end();)
    {
        if (attrib->def_index() == attributeStickerId
            || attrib->def_index() == attributeStickerWear)
        {
            attrib = item.mutable_attribute()->erase(attrib);
        }
        else
        {
            attrib++;
        }
    }
}

bool Inventory::ScrapeSticker(const CMsgApplySticker &message,
    CMsgSOSingleObject &update,
    CMsgSOSingleObject &destroy,
    CMsgGCItemCustomizationNotification &notification)
{
    auto it = m_items.find(message.item_item_id());
    if (it == m_items.end())
    {
        assert(false);
        return false;
    }

    CSOEconItem &item = it->second;

    // mikkotodo lookup table instead of this crap...
    uint32_t attributeStickerWear = ItemSchema::AttributeStickerWear0 + (message.sticker_slot() * 4);

    CSOEconItemAttribute *wearAttribute = nullptr;
    for (int i = 0; i < item.attribute_size(); i++)
    {
        if (item.mutable_attribute(i)->def_index() == attributeStickerWear)
        {
            wearAttribute = item.mutable_attribute(i);
            break;
        }
    }

    float wearLevel = 0.0f;

    if (wearAttribute)
    {
        // mikkotodo randomize
        float wearIncrement = 1.0f / 9;
        wearLevel = m_itemSchema.AttributeFloat(wearAttribute) + wearIncrement;
    }

    // if the wear attribute is not present, remove it outright (patches)
    if (!wearAttribute || wearLevel > 1.0f)
    {
        // so long, and thanks for all the fish

        // mikkotodo fix... should this be deduced from the item???
        uint32_t request = k_EGCItemCustomizationNotification_RemoveSticker;
        if (!wearAttribute)
        {
            request = k_EGCItemCustomizationNotification_RemovePatch;
        }

        if (item.rarity() == ItemSchema::RarityDefault)
        {
            // sticker removal notification with a fake item id
            notification.add_item_id(item.def_index() | ItemIdDefaultItemMask);
            notification.set_request(request);

            // this was a default weapon clone with a sticker so destroy the entire item
            DestroyItem(it, destroy);
        }
        else
        {
            // sticker removal notification
            notification.add_item_id(item.id());
            notification.set_request(request);

            // remove the sticker
            RemoveStickerAttributes(item, message.sticker_slot());

            ToSingleObject(update, item);
        }
    }
    else
    {
        // just update the wear
        m_itemSchema.SetAttributeFloat(wearAttribute, wearLevel);

        ToSingleObject(update, item);
    }

    return true;
}

bool Inventory::IncrementKillCountAttribute(uint64_t itemId, uint32_t amount, CMsgSOSingleObject &update)
{
    auto it = m_items.find(itemId);
    if (it == m_items.end())
    {
        assert(false);
        return false;
    }

    CSOEconItem &item = it->second;
    bool incremented = false;

    for (int i = 0; i < item.attribute_size(); i++)
    {
        CSOEconItemAttribute *attribute = item.mutable_attribute(i);
        if (attribute->def_index() == ItemSchema::AttributeKillEater)
        {
            int value = m_itemSchema.AttributeUint32(attribute) + amount;
            m_itemSchema.SetAttributeUint32(attribute, value);
            incremented = true;
            break;
        }
    }

    if (incremented)
    {
        ToSingleObject(update, item);
        return true;
    }

    assert(false);
    return false;
}

bool Inventory::NameItem(uint64_t nameTagId,
    uint64_t itemId,
    std::string_view name,
    CMsgSOSingleObject &update,
    CMsgSOSingleObject &destroy,
    CMsgGCItemCustomizationNotification &notification)
{
    auto it = m_items.find(itemId);
    if (it == m_items.end())
    {
        assert(false);
        return false;
    }

    it->second.mutable_custom_name()->assign(name);

    ToSingleObject(update, it->second);

    if (m_config.DestroyUsedItems())
    {
        auto tag = m_items.find(nameTagId);
        if (tag == m_items.end())
        {
            assert(false);
            return false;
        }

        DestroyItem(tag, destroy);
    }

    notification.add_item_id(it->second.id());
    notification.set_request(k_EGCItemCustomizationNotification_NameItem);

    return true;
}

bool Inventory::NameBaseItem(uint64_t nameTagId,
    uint32_t defIndex,
    std::string_view name,
    CMsgSOSingleObject &create,
    CMsgSOSingleObject &destroy,
    CMsgGCItemCustomizationNotification &notification)
{
    CSOEconItem &item = CreateItem(defIndex, ItemOriginBaseItem, UnacknowledgedInvalid);

    item.mutable_custom_name()->assign(name);

    ToSingleObject(create, item);

    if (m_config.DestroyUsedItems())
    {
        auto tag = m_items.find(nameTagId);
        if (tag == m_items.end())
        {
            assert(false);
            return false;
        }

        DestroyItem(tag, destroy);
    }

    notification.add_item_id(item.id()); // mikkotodo def index???
    notification.set_request(k_EGCItemCustomizationNotification_NameBaseItem);

    return true;
}

bool Inventory::RemoveItemName(uint64_t itemId,
    CMsgSOSingleObject &update,
    CMsgSOSingleObject &destroy,
    CMsgGCItemCustomizationNotification &notification)
{
    auto it = m_items.find(itemId);
    if (it == m_items.end())
    {
        assert(false);
        return false;
    }

    if (it->second.rarity() == ItemSchema::RarityDefault)
    {
        notification.add_item_id(it->second.def_index() | ItemIdDefaultItemMask);
        notification.set_request(k_EGCItemCustomizationNotification_RemoveItemName);

        DestroyItem(it, destroy);
    }
    else
    {
        it->second.mutable_custom_name()->clear();

        notification.add_item_id(it->second.id());
        notification.set_request(k_EGCItemCustomizationNotification_RemoveItemName);

        ToSingleObject(update, it->second);
    }

    return true;
}

uint64_t Inventory::PurchaseItem(uint32_t defIndex, std::vector<CMsgSOSingleObject> &update)
{
    CSOEconItem &item = CreateItem(defIndex, ItemOriginPurchased, UnacknowledgedPurchased);

    CMsgSOSingleObject &single = update.emplace_back();
    ToSingleObject(single, item);

    return item.id();
}

bool Inventory::UnequipItem(uint64_t itemId, CMsgSOMultipleObjects &update)
{
    uint32_t defIndex, paintKitIndex;
    if (IsDefaultItemId(itemId, defIndex, paintKitIndex))
    {
        // not supported
        assert(false);
        return false;
    }

    auto it = m_items.find(itemId);
    if (it == m_items.end())
    {
        assert(false);
        return false;
    }

    CSOEconItem &item = it->second;
    item.clear_equipped_state();

    AddToMultipleObjects(update, item);

    return true;
}

// this goes through everything on purpose
void Inventory::UnequipItem(uint32_t classId, uint32_t slotId, CMsgSOMultipleObjects &update)
{
    // check non default items first
    for (auto &pair : m_items)
    {
        CSOEconItem &item = pair.second;

        bool modified = false;

        for (auto it = item.mutable_equipped_state()->begin(); it != item.mutable_equipped_state()->end();)
        {
            if (it->new_class() == classId && it->new_slot() == slotId)
            {
                Platform::Print("Unequip %llu class %d slot %d\n", pair.first, classId, slotId);

                it = item.mutable_equipped_state()->erase(it);
                modified = true;
            }
            else
            {
                it++;
            }
        }

        if (modified)
        {
            AddToMultipleObjects(update, item);
        }
    }

    // check default equips
    for (auto it = m_defaultEquips.begin(); it != m_defaultEquips.end();)
    {
        if (it->class_id() == classId && it->slot_id() == slotId)
        {
            Platform::Print("Unequip %u class %d slot %d\n", it->item_definition(), classId, slotId);

            // mikkotodo is this correct???
            // mikkotodo rpobably not correct.. i gess we don't even have to do this
            // because the new equip overrides the old one
            // but we can't just remove it either because "update" would get fucked
            it->set_item_definition(0);
            AddToMultipleObjects(update, *it);

            it = m_defaultEquips.erase(it);
        }
        else
        {
            it++;
        }
    }
}

void Inventory::DestroyItem(ItemMap::iterator iterator, CMsgSOSingleObject &message)
{
    CSOEconItem item;
    item.set_id(iterator->second.id());

    ToSingleObject(message, item);

    m_items.erase(iterator);
}
