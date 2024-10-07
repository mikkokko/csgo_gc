#include "stdafx.h"
#include "inventory.h"
#include "gc_const.h"
#include "gc_const_csgo.h"
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
    if (itemId & ItemIdDefaultItemMask)
    {
        defIndex = itemId & 0xffff;
        paintKitIndex = (itemId >> 16) & 0xffff;
        return true;
    }

    return false;
}

Inventory::Inventory(uint64_t steamId)
    : m_steamId{ steamId }
{
    ReadFromFile();
}

Inventory::~Inventory()
{
    WriteToFile();
}

uint32_t Inventory::AccountId() const
{
    return m_steamId & 0xffffffff;
}

// Players fuck up their inventory files constantly and end up with item id collisions...
// This doesn't return until the item id is unique for this session, try with the provided
// item id first, if it's invalid or already in use increment it
CSOEconItem &Inventory::CreateItem(uint32_t highItemId, CSOEconItem *copyFrom)
{
    if (!highItemId)
    {
        m_lastGeneratedHighItemId++;
        highItemId = m_lastGeneratedHighItemId;
    }

    for (; ; highItemId++)
    {
        uint64_t itemId = ComposeItemId(AccountId(), highItemId);
        if (itemId & ItemIdDefaultItemMask)
        {
            // would be interpreted as a default item (it's not)
            assert(false);
            // shit error handling
            continue;
        }

        auto result = m_items.try_emplace(itemId);
        if (!result.second)
        {
            // item id collision
            assert(false);
            continue;
        }

        if (highItemId > m_lastGeneratedHighItemId)
        {
            m_lastGeneratedHighItemId = highItemId;
        }

        // ok
        CSOEconItem &item = result.first->second;

        if (copyFrom)
        {
            item = *copyFrom;
        }

        item.set_id(itemId);
        item.set_account_id(AccountId());

        return item;
    }
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
            assert(highItemId);
            CSOEconItem &item = CreateItem(highItemId);
            ReadItem(itemKey, item);
        }
    }

    // find the largest item id for generating new items
    for (const auto &pair : m_items) // mikkotodo remove this code path
    {
        const CSOEconItem &item = pair.second;
        uint32_t highItemId = HighItemId(item.id());

        if (highItemId > m_lastGeneratedHighItemId)
        {
            assert(false);
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

    std::string_view desc = itemKey.GetString("custom_desc");
    if (desc.size())
    {
        item.set_custom_desc(std::string{ desc });
    }

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

            if (m_itemSchema.AttributeStoredAsInteger(defIndex))
            {
                int value = FromString<int>(attributeKey.String());
                attribute->set_value_bytes(&value, sizeof(value));
            }
            else
            {
                float value = FromString<float>(attributeKey.String());
                attribute->set_value_bytes(&value, sizeof(value));
            }
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
    itemKey.AddString("custom_desc", item.custom_desc());

    itemKey.AddNumber("in_use", item.in_use());
    //itemKey.AddNumber("style", item.style());
    //itemKey.AddNumber("original_id", item.original_id());
    itemKey.AddNumber("rarity", item.rarity());

    KeyValue &attributesKey = itemKey.AddSubkey("attributes");
    for (const CSOEconItemAttribute &attribute : item.attribute())
    {
        if (m_itemSchema.AttributeStoredAsInteger(attribute.def_index()))
        {
            int value = *reinterpret_cast<const int *>(attribute.value_bytes().data());
            attributesKey.AddNumber(std::to_string(attribute.def_index()), value);
        }
        else
        {
            float value = *reinterpret_cast<const float *>(attribute.value_bytes().data());
            attributesKey.AddNumber(std::to_string(attribute.def_index()), value);
        }
    }

    KeyValue &equippedStateKey = itemKey.AddSubkey("equipped_state");
    for (const CSOEconItemEquipped &equip : item.equipped_state())
    {
        equippedStateKey.AddNumber(std::to_string(equip.new_class()), equip.new_slot());
    }
}

void Inventory::BuildCacheSubscription(CMsgSOCacheSubscribed &message, int level, bool server)
{
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
        // CSOEconDefaultEquippedDefinitionInstanceClient

        CMsgSOCacheSubscribed_SubscribedType *object = message.add_objects();
        object->set_type_id(SOTypeDefaultEquippedDefinitionInstanceClient);

        for (const CSOEconDefaultEquippedDefinitionInstanceClient &defaultEquip : m_defaultEquips)
        {
            object->add_object_data(defaultEquip.SerializeAsString());
        }
    }

    message.set_version(InventoryVersion);
    message.mutable_owner_soid()->set_type(SoIdTypeSteamId);
    message.mutable_owner_soid()->set_id(m_steamId);
}

// mikkotodo move
constexpr uint32_t SlotUneqip = 0xffff;
constexpr uint64_t ItemIdInvalid = 0;

// yes this function is inefficent!!! but i think that makes it more clear
// also i think this is the way valve gc does it???? can't remember
bool Inventory::EquipItem(uint64_t itemId, uint32_t classId, uint32_t slotId, CMsgSOMultipleObjects &update)
{
    // setup this... so i dont forget...
    update.set_version(InventoryVersion); // mikkotodo is this correct
    update.mutable_owner_soid()->set_type(SoIdTypeSteamId);
    update.mutable_owner_soid()->set_id(m_steamId);

    if (slotId == SlotUneqip)
    {
        // unequipping a specific item from **all** slots
        // Platform::Print("Uneqip only, done!!!\n");
        // return UnequipItem(message.item_id(), update);
        assert(false);
        return false;
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

        CMsgSOMultipleObjects_SingleObject *object = update.add_objects_modified();
        object->set_type_id(SOTypeDefaultEquippedDefinitionInstanceClient);
        object->set_object_data(defaultEquip.SerializeAsString());

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

        CMsgSOMultipleObjects_SingleObject *object = update.add_objects_modified();
        object->set_type_id(SOTypeItem);
        object->set_object_data(item.SerializeAsString());

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

    if (it->second.def_index() != m_itemSchema.SealedGraffitiDefIndex())
    {
        assert(false);
        return false;
    }

    // create an unsealed spray based on the sealed one
    CSOEconItem &unsealed = CreateItem(0, &it->second);
    unsealed.set_def_index(m_itemSchema.GraffitiDefIndex());

    // remove the sealed spray from our inventory
    DestroyItem(it, destroy);

    // equip the new spray, this will also unequip the old one if we had one
    EquipItem(unsealed.id(), 0, m_itemSchema.GraffitiSlot(), updateMultiple);

    // set notification
    notification.add_item_id(unsealed.id());
    notification.set_request(k_EGCItemCustomizationNotification_GraffitiUnseal);

    return true;
}

// mikkotodo document
constexpr uint32_t InventoryFieldCase()
{
    return 5u | (1u << 30);
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

    auto key = m_items.find(keyId);
    if (key == m_items.end())
    {
        assert(false);
        return false;
    }

    DestroyItem(crate, destroyCrate);
    DestroyItem(key, destroyKey);

    // new item (mikkotodo actually implement...)
    CSOEconItem &unlocked = CreateItem(0);
    unlocked.set_inventory(InventoryFieldCase()); // set when the game sends us a k_EMsgGCSetItemPositions
    unlocked.set_def_index(63);
    unlocked.set_quantity(1);
    unlocked.set_level(1);
    unlocked.set_quality(4);
    unlocked.set_flags(0);
    unlocked.set_origin(8);
    unlocked.set_in_use(0);
    unlocked.set_rarity(1);
    CSOEconItemAttribute *attribute = unlocked.add_attribute();
    attribute->set_def_index(6);
    float value = 218;
    attribute->set_value_bytes(&value, sizeof(value));

    {
        newItem.set_version(InventoryVersion);
        newItem.mutable_owner_soid()->set_type(SoIdTypeSteamId);
        newItem.mutable_owner_soid()->set_id(m_steamId);
        newItem.set_type_id(SOTypeItem);
        newItem.set_object_data(unlocked.SerializeAsString());
    }

    // set notification
    notification.add_item_id(unlocked.id());
    notification.set_request(k_EGCItemCustomizationNotification_UnlockCrate);

    return true;
}

// mikkotodo incomplete
static void ItemToPreviewDataBlock(const CSOEconItem &item, CEconItemPreviewDataBlock &block)
{
    block.set_accountid(item.account_id());
    block.set_itemid(item.id());
    block.set_defindex(item.def_index());
    //block.set_paintindex(item.paintindex());
    block.set_rarity(item.rarity());
    block.set_quality(item.quality());
    //block.set_paintwear(item.paintwear());
    //block.set_paintseed(item.paintseed());
    //block.set_killeaterscoretype(item.killeaterscoretype());
    //block.set_killeatervalue(item.killeatervalue());
    block.set_customname(item.custom_name());
    //block.set_stickers(item.stickers());
    block.set_inventory(item.inventory());
    block.set_origin(item.origin());
    //block.set_questid(item.questid());
    //block.set_dropreason(item.dropreason());
    //block.set_musicindex(item.musicindex());
    //block.set_entindex(item.entindex());
}

bool Inventory::SetItemPositions(
    const CMsgSetItemPositions &message,
    std::vector<CMsgItemAcknowledged> &acknowledgements,
    CMsgSOMultipleObjects &update)
{
    update.set_version(InventoryVersion);
    update.mutable_owner_soid()->set_type(SoIdTypeSteamId);
    update.mutable_owner_soid()->set_id(m_steamId);

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

        CMsgSOMultipleObjects_SingleObject *object = update.add_objects_modified();
        object->set_type_id(SOTypeItem);
        object->set_object_data(item.SerializeAsString());
    }

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
            CMsgSOMultipleObjects_SingleObject *object = update.add_objects_modified();
            object->set_type_id(SOTypeItem);
            object->set_object_data(item.SerializeAsString());
        }
    }

    // check default equips
    for (auto it = m_defaultEquips.begin(); it != m_defaultEquips.end();)
    {
        if (it->class_id() == classId && it->slot_id() == slotId)
        {
            Platform::Print("Unequip %u class %d slot %d\n", it->item_definition(), classId, slotId);

            // mikkotodo is this correct???
            it->set_item_definition(0);

            CMsgSOMultipleObjects_SingleObject *object = update.add_objects_modified();
            object->set_type_id(SOTypeDefaultEquippedDefinitionInstanceClient);
            object->set_object_data(it->SerializeAsString());

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

    message.set_version(InventoryVersion);
    message.mutable_owner_soid()->set_type(SoIdTypeSteamId);
    message.mutable_owner_soid()->set_id(m_steamId);
    message.set_type_id(SOTypeItem);
    message.set_object_data(item.SerializeAsString());

    m_items.erase(iterator);
}
