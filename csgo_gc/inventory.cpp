#include "stdafx.h"
#include "inventory.h"
#include "case_opening.h"
#include "config.h"
#include "gc_const.h"
#include "inventory_modify.h"
#include "keyvalue.h"
#include "random.h"

#include "base_gcmessages.pb.h"
#include "cstrike15_gcmessages.pb.h"

constexpr const char *InventoryFilePath = "csgo_gc/inventory.txt";

// mikkotodo actual versioning
constexpr uint64_t InventoryVersion = 7523377975160828514;

// if the high item id is higher than this, it'll get interpreted as a default item fake id
constexpr uint32_t MaxHighItemId = static_cast<uint32_t>((ItemIdDefaultItemMask >> 32) - 1);

// mikkotodo move
constexpr uint32_t SlotUneqip = 0xffff;

// mix the account id into item ids to avoid collisions in multiplayer games
inline uint64_t ComposeItemId(uint32_t accountId, uint32_t highItemId)
{
    uint64_t low = accountId;
    uint64_t high = highItemId;
    return low | (high << 32);
}

// get the full item id of the casket item is in
static uint64_t GetCasketId(const Item &item)
{
    uint64_t high = item.GetAttributeValue<uint32_t>(AttributeDefIndex::CasketIdHigh);
    return high ? ((high << 32) | item.GetAttributeValue<uint32_t>(AttributeDefIndex::CasketIdLow)) : 0;
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

inline void DefaultEquipToProto(
    CSOEconDefaultEquippedDefinitionInstanceClient &proto,
    const DefaultEquip &defaultEquip,
    uint32_t accountId)
{
    proto.set_account_id(accountId);
    proto.set_item_definition(defaultEquip.defIndex);
    proto.set_class_id(defaultEquip.classId);
    proto.set_slot_id(defaultEquip.slotId);
}

Inventory::Inventory(uint64_t steamId)
    : m_steamId{ steamId }
    , m_editor{ m_itemSchema }
{
    ReadFromFile();
}

Inventory::~Inventory()
{
    WriteToFile();
}

bool Inventory::Update(InventoryChangeMessages &changeMessages)
{
    EditorChanges changes;
    const bool hasChanges = m_editor.GetChanges(changes);

    if (m_editor.WantsFullInventory())
    {
        SendFullInventoryToEditor();
    }

    if (!hasChanges)
    {
        return false;
    }

    // FIXME: this will cause the changes to be sent back to the editor!!! ideally we would not do this,
    // but we have cases like removal of caskets or casketed items that will cause more item modifications
    InventoryModify modify{ *this };

    for (const EditorItem &editorItem : changes.items)
    {
        if (editorItem.type == EditorItemChangeType::Destroyed)
        {
            Item *item = FindItem(ComposeItemId(AccountId(), editorItem.highId));
            if (item)
            {
                DestroyItem(modify, item);
            }
            else
            {
                assert(false);
            }

            continue;
        }

        if (editorItem.type == EditorItemChangeType::Created)
        {
            // editorItem.highId is NOT the item id we should use!!!!
            // it's an opaque "item request id" from the editor, we
            // need to send the actual item id back now that we've determined it
            Item &item = CreateItem(modify, editorItem.desc);
            m_editor.SendItemId(editorItem.highId, item.HighId());
            continue;
        }

        // modification
        Item *item = FindItem(ComposeItemId(AccountId(), editorItem.highId));
        if (!item)
        {
            assert(false);
            continue;
        }

        modify.UpdateFromDesc(*item, editorItem.desc);
    }

    changeMessages = BuildChangeMessages(modify);
    return true;
}

void Inventory::FlushChanges(const InventoryModify &modify)
{
    if (modify.m_itemChanges.empty() && modify.m_defaultEquipChanges.empty())
    {
        // nope
        return;
    }

    SendChangesToEditor(modify);

    WriteToFile();
}

void Inventory::SendFullInventoryToEditor()
{
    EditorChanges changes;

    m_editorVisibleItems.clear();

    for (const auto &pair : m_items)
    {
        if (GetCasketId(pair.second))
        {
            // don't expose casketed items to the editor
            continue;
        }

        EditorItem &item = changes.items.emplace_back();
        item.type = EditorItemChangeType::Created;
        item.highId = pair.second.HighId();
        pair.second.ToDesc(item.desc);

        m_editorVisibleItems.insert(item.highId);
    }

    m_editor.SendChanges(changes, true);
}

void Inventory::SendChangesToEditor(const InventoryModify &modify)
{
    EditorChanges changes;

    for (const auto &[highId, change] : modify.m_itemChanges)
    {
        const Item *source = FindItem(ComposeItemId(AccountId(), highId));
        if (!source || GetCasketId(*source))
        {
            if (m_editorVisibleItems.erase(highId))
            {
                EditorItem &dest = changes.items.emplace_back();
                dest.highId = highId;
                dest.type = EditorItemChangeType::Destroyed;
            }

            continue;
        }

        bool inserted = m_editorVisibleItems.insert(highId).second;

        EditorItem &dest = changes.items.emplace_back();
        dest.highId = highId;
        dest.type = inserted ? EditorItemChangeType::Created : EditorItemChangeType::Modified;
        source->ToDesc(dest.desc);
    }

    m_editor.SendChanges(changes, false);
}

InventoryChangeMessages Inventory::BuildChangeMessages(
    const InventoryModify &modify,
    std::optional<EGCItemCustomizationNotification> customizationType,
    std::initializer_list<uint64_t> customizationItemIds)
{
    InventoryChangeMessages messages;

    for (auto [highId, change] : modify.m_itemChanges)
    {
        uint64_t itemId = ComposeItemId(AccountId(), highId);

        if (change.type == ItemChangeType::Destroyed)
        {
            assert(!FindItem(itemId));

            CSOEconItem econItem;
            econItem.set_id(itemId);

            SingleObject &object = messages.destroyed.emplace_back();
            object.sendToGameServer = change.gameServerDirty;
            ToSingleObject(object.proto, SOTypeItem, econItem);
            continue;
        }

        Item *item = FindItem(itemId);
        if (!item)
        {
            assert(false);
            continue;
        }

        CSOEconItem econItem;
        item->ToCSOEconItem(econItem, AccountId());

        if (change.type == ItemChangeType::Created)
        {
            SingleObject &object = messages.created.emplace_back();
            object.sendToGameServer = change.gameServerDirty;
            ToSingleObject(object.proto, SOTypeItem, econItem);
        }
        else
        {
            AddToMultipleObjects(messages.updatedClient, SOTypeItem, econItem);

            if (change.gameServerDirty)
            {
                AddToMultipleObjects(messages.updatedGameServer, SOTypeItem, econItem);
            }
        }
    }

    for (const DefaultEquip &defaultEquip : modify.m_defaultEquipChanges)
    {
        CSOEconDefaultEquippedDefinitionInstanceClient proto;
        DefaultEquipToProto(proto, defaultEquip, AccountId());
        AddToMultipleObjects(messages.updatedClient, SOTypeDefaultEquippedDefinitionInstanceClient, proto);
        AddToMultipleObjects(messages.updatedGameServer, SOTypeDefaultEquippedDefinitionInstanceClient, proto);
    }

    if (customizationType.has_value())
    {
        messages.notification.mutable_item_id()->Assign(customizationItemIds.begin(), customizationItemIds.end());
        messages.notification.set_request(*customizationType);
    }

    return messages;
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
    object.SerializeToString(single->mutable_object_data());
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
    object.SerializeToString(message.mutable_object_data());
}

uint32_t Inventory::AccountId() const
{
    return m_steamId & 0xffffffff;
}

uint32_t Inventory::GetHighItemId(uint32_t highItemId)
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
        if (highItemId > MaxHighItemId)
        {
            // would be interpreted as a default item (it's not)
            // item ids should be strictly monotonically increasing, but
            // if the situation gets this cooked then we need to reset
            m_lastHighItemId = 1;
            highItemId = m_lastHighItemId;
        }

        if (m_items.count(ComposeItemId(AccountId(), highItemId)))
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
        return highItemId;
    }
}

void Inventory::CreateItem(uint32_t highId, const KeyValue &kv)
{
    highId = GetHighItemId(highId);
    uint64_t itemId = ComposeItemId(AccountId(), highId);

    // this will succeed, GetHighItemId confirmed there are no collisions
    auto [it, inserted] = m_items.try_emplace(itemId, highId, AccountId(), kv, m_itemSchema);
    assert(inserted);
}

Item &Inventory::CreateItem(InventoryModify &modify, const ItemDesc &desc)
{
    uint32_t highId = GetHighItemId(0);
    uint64_t itemId = ComposeItemId(AccountId(), highId);

    // this will succeed, GetHighItemId confirmed there are no collisions
    auto [it, inserted] = m_items.try_emplace(itemId, highId, desc);
    assert(inserted);

    modify.MarkItemCreated(highId, it->second.HasEquips());

    return it->second;
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
            CreateItem(highItemId, itemKey);
        }
    }

    const KeyValue *defaultEquipsKey = inventoryKey.GetSubkey("default_equips");
    if (defaultEquipsKey)
    {
        m_defaultEquips.reserve(defaultEquipsKey->SubkeyCount());

        for (const KeyValue &defaultEquipKey : *defaultEquipsKey)
        {
            uint32_t defIndex = FromString<uint32_t>(defaultEquipKey.Name());
            uint32_t classId = defaultEquipKey.GetNumber<uint32_t>("class_id");
            uint32_t slotId = defaultEquipKey.GetNumber<uint32_t>("slot_id");
            m_defaultEquips.emplace_back(defIndex, classId, slotId);
        }
    }
}

void Inventory::WriteToFile() const
{
    Platform::Print("Writing inventory to {} ({} items, {} default equips)\n",
        InventoryFilePath,
        m_items.size(),
        m_defaultEquips.size());

    KeyValue inventoryKey{ "inventory" };

    {
        KeyValue &itemsKey = inventoryKey.AddSubkey("items");

        for (const auto &pair : m_items)
        {
            const Item &item = pair.second;
            KeyValue &itemKey = itemsKey.AddSubkey(std::to_string(item.HighId()));
            item.ToKeyValue(itemKey);
        }
    }

    {
        KeyValue &defaultEquipsKey = inventoryKey.AddSubkey("default_equips");

        for (const DefaultEquip &defaultEquip : m_defaultEquips)
        {
            KeyValue &defaultEquipKey = defaultEquipsKey.AddSubkey(std::to_string(defaultEquip.defIndex));
            defaultEquipKey.AddNumber("class_id", defaultEquip.classId);
            defaultEquipKey.AddNumber("slot_id", defaultEquip.slotId);
        }
    }

    inventoryKey.WriteToFile(InventoryFilePath);
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
            if (server && !pair.second.HasEquips())
            {
                continue;
            }

            CSOEconItem serialized;
            pair.second.ToCSOEconItem(serialized, AccountId());
            serialized.SerializeToString(object->add_object_data());
        }
    }

    {
        CSOPersonaDataPublic personaData;
        personaData.set_player_level(level);
        personaData.set_elevated_state(true);

        CMsgSOCacheSubscribed_SubscribedType *object = message.add_objects();
        object->set_type_id(SOTypePersonaDataPublic);
        personaData.SerializeToString(object->add_object_data());
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
        accountClient.SerializeToString(object->add_object_data());
    }

    {
        CMsgSOCacheSubscribed_SubscribedType *object = message.add_objects();
        object->set_type_id(SOTypeDefaultEquippedDefinitionInstanceClient);

        for (const DefaultEquip &defaultEquip : m_defaultEquips)
        {
            CSOEconDefaultEquippedDefinitionInstanceClient proto;
            DefaultEquipToProto(proto, defaultEquip, AccountId());
            proto.SerializeToString(object->add_object_data());
        }
    }
}

// yes this function is inefficent!!! but i think that makes it more clear
// also i think this is the way valve gc does it???? can't remember
InventoryChangeMessages Inventory::EquipItem(uint64_t itemId, uint32_t classId, uint32_t slotId)
{
    InventoryModify modify{ *this };
    if (!EquipItem(modify, itemId, classId, slotId))
    {
        assert(false);
        return {};
    }

    return BuildChangeMessages(modify);
}

bool Inventory::EquipItem(InventoryModify &modify, uint64_t itemId, uint32_t classId, uint32_t slotId)
{
    if (slotId == SlotUneqip)
    {
        // unequipping a specific item from all slots
        return UnequipItem(modify, itemId);
    }

    // mikkotodo cleanup, old junk
    assert(itemId != UINT64_MAX); // probably an old csgo thing

    if (!itemId)
    {
        // unequip from this slot, itemid not provided so nothing gets equipped
        UnequipItem(modify, classId, slotId);
        return true;
    }

    uint32_t defIndex, paintKitIndex;
    if (IsDefaultItemId(itemId, defIndex, paintKitIndex))
    {
        // if an item is equipped in this slot, unequip it first
        UnequipItem(modify, classId, slotId);

        Platform::Print("EquipItem def {} class {} slot {}\n", defIndex, classId, slotId);

        DefaultEquip &defaultEquip = m_defaultEquips.emplace_back(defIndex, classId, slotId);
        modify.MarkDefaultEquipChanged(defaultEquip);

        return true;
    }

    Item *item = FindItem(itemId);
    if (!item)
    {
        Platform::Print("EquipItem: no such item {}!!!!\n", itemId);
        return false; // didn't modify anything
    }

    // if an item is equipped in this slot, unequip it first
    UnequipItem(modify, classId, slotId);

    Platform::Print("EquipItem {} class {} slot {}\n", itemId, classId,
        slotId);

    modify.AddItemEquip(*item, classId, slotId);

    return true;
}

InventoryChangeMessages Inventory::RemoveItem(uint64_t itemId)
{
    InventoryModify modify{ *this };
    if (!DestroyItemById(modify, itemId))
    {
        assert(false);
        return {};
    }

    return BuildChangeMessages(modify);
}

InventoryChangeMessages Inventory::UseItem(uint64_t itemId)
{
    InventoryModify modify{ *this };
    Item *item = FindItem(itemId);
    if (!item)
    {
        assert(false);
        return {};
    }

    if (item->DefIndex() != ItemDefIndex::Spray)
    {
        assert(false);
        return {};
    }

    // create an unsealed spray based on the sealed one
    ItemDesc temp;
    item->ToDesc(temp);
    temp.defIndex = ItemDefIndex::SprayPaint;
    Item &unsealed = CreateItem(modify, temp);
    uint64_t unsealedId = unsealed.FullIdFor(AccountId());

    // remove the sealed spray from our inventory
    DestroyItem(modify, item);

    // equip the new spray, this will also unequip the old one if we had one
    EquipItem(modify, unsealedId, 0, LoadoutSlotGraffiti);

    // remove this to have unlimited sprays
    modify.SetItemAttribute(unsealed, AttributeDefIndex::SpraysRemaining, 50u);

    return BuildChangeMessages(modify, k_EGCItemCustomizationNotification_GraffitiUnseal, { unsealedId });
}

InventoryChangeMessages Inventory::UnlockCrate(uint64_t crateId, uint64_t keyId)
{
    Item *crate = FindItem(crateId);
    if (!crate)
    {
        assert(false);
        return {};
    }

    // CASE OPENING
    CaseOpening caseOpening{ m_itemSchema, m_random };

    ItemDesc temp;
    if (!caseOpening.SelectItemFromCrate(crate->DefIndex(), temp))
    {
        assert(false);
        return {};
    }

    InventoryModify modify{ *this };
    Item &item = CreateItem(modify, temp);
    uint64_t itemId = item.FullIdFor(AccountId());

    if (GetConfig().DestroyUsedItems())
    {
        assert(keyId != itemId);
        DestroyItem(modify, crate);
        DestroyItemById(modify, keyId);
    }

    return BuildChangeMessages(modify, k_EGCItemCustomizationNotification_UnlockCrate, { itemId });
}

InventoryChangeMessages Inventory::SetItemPositions(
    const CMsgSetItemPositions &message,
    std::vector<CMsgItemAcknowledged> &acknowledgements)
{
    for (const CMsgSetItemPositions_ItemPosition &position : message.item_positions())
    {
        if (!FindItem(position.item_id()))
        {
            assert(false);
            return {};
        }
    }

    InventoryModify modify{ *this };

    for (const CMsgSetItemPositions_ItemPosition &position : message.item_positions())
    {
        Item *item = FindItem(position.item_id());

        Platform::Print("SetItemPositions: {} --> {}\n", position.item_id(), position.position());
        modify.SetItemInventory(*item, position.position());

        CMsgItemAcknowledged &acknowledgement = acknowledgements.emplace_back();
        item->ToEconItemPreviewDataBlock(*acknowledgement.mutable_iteminfo(), AccountId());
    }

    return BuildChangeMessages(modify);
}

static AttributeDefIndex StickerAttributeForSlot(AttributeDefIndex defIndex, uint32_t slot)
{
    // offset by id, wear, scale, rotation
    uint32_t result = FromEnum(defIndex) + (slot * 4);
    return ToEnum<AttributeDefIndex>(result);
}

InventoryChangeMessages Inventory::ApplySticker(const CMsgApplySticker &message)
{
    InventoryModify modify{ *this };

    assert(message.has_sticker_item_id());
    assert(message.has_sticker_slot());
    assert(!message.has_sticker_wear());

    Item *sticker = FindItem(message.sticker_item_id());
    if (!sticker)
    {
        assert(false);
        return {};
    }

    Item *item = nullptr;
    if (!message.baseitem_defidx())
    {
        item = FindItem(message.item_item_id());
        if (!item)
        {
            assert(false);
            return {};
        }
    }

    // get the sticker kit def index
    uint32_t stickerKit = sticker->GetAttributeValue<uint32_t>(AttributeDefIndex::StickerId0);
    if (!stickerKit)
    {
        assert(false);
        return {};
    }

    if (!item)
    {
        assert(message.baseitem_defidx() && !message.item_item_id());

        ItemDesc desc{};
        m_itemSchema.GetItemDesc(ToEnum<ItemDefIndex>(message.baseitem_defidx()), ItemOriginBaseItem, UnacknowledgedInvalid, desc);
        item = &CreateItem(modify, desc);
    }
    else
    {
        assert(!message.baseitem_defidx() && message.item_item_id());
    }

    auto attributeStickerId = StickerAttributeForSlot(AttributeDefIndex::StickerId0, message.sticker_slot());
    auto attributeStickerWear = StickerAttributeForSlot(AttributeDefIndex::StickerWear0, message.sticker_slot());

    // add the sticker id attribute
    modify.SetItemAttribute(*item, attributeStickerId, stickerKit);

    // add the sticker wear attribute if this is not a patch (mikkotodo revisit...)
    if (sticker->DefIndex() != ItemDefIndex::Patch)
    {
        modify.SetItemAttribute(*item, attributeStickerWear, 0.0f);
    }

    uint64_t itemId = item->FullIdFor(AccountId());

    if (GetConfig().DestroyUsedItems())
    {
        DestroyItem(modify, sticker);
    }

    return BuildChangeMessages(modify, k_EGCItemCustomizationNotification_ApplySticker, { itemId });
}

static void RemoveStickerAttributes(InventoryModify &modify, Item &item, uint32_t slot)
{
    // mikkotodo rest of attribs???
    auto attributeStickerId = StickerAttributeForSlot(AttributeDefIndex::StickerId0, slot);
    auto attributeStickerWear = StickerAttributeForSlot(AttributeDefIndex::StickerWear0, slot);
    modify.RemoveItemAttributes(item, { attributeStickerId, attributeStickerWear });
}

InventoryChangeMessages Inventory::ScrapeSticker(const CMsgApplySticker &message)
{
    InventoryModify modify{ *this };
    Item *item = FindItem(message.item_item_id());
    if (!item)
    {
        assert(false);
        return {};
    }

    auto attributeStickerWear = StickerAttributeForSlot(AttributeDefIndex::StickerWear0, message.sticker_slot());

    // mikkotodo randomize
    float wearIncrement = 1.0f / 9;

    // increment the wear, if there was no war (patches) or it went over 1, remove the sticker/patch
    auto wearUpdate = modify.IncrementItemAttribute(*item, attributeStickerWear, wearIncrement, 0.0f, 1.0f);
    if (wearUpdate != AttributeIncrement::Ok)
    {
        // mikkotodo fix... should this be deduced from the item???
        EGCItemCustomizationNotification request = k_EGCItemCustomizationNotification_RemoveSticker;
        if (wearUpdate == AttributeIncrement::NoAttribute)
        {
            request = k_EGCItemCustomizationNotification_RemovePatch;
        }

        if (item->GetRarity() == Rarity::Default)
        {
            // this was a default weapon clone with a sticker so destroy the entire item
            uint64_t fakeItemId = static_cast<uint64_t>(item->DefIndex()) | ItemIdDefaultItemMask;
            DestroyItem(modify, item);
            return BuildChangeMessages(modify, request, { fakeItemId });
        }

        // remove the sticker
        RemoveStickerAttributes(modify, *item, message.sticker_slot());

        return BuildChangeMessages(modify, request, { item->FullIdFor(AccountId()) });
    }

    return BuildChangeMessages(modify);
}

InventoryChangeMessages Inventory::IncrementKillCountAttribute(uint64_t itemId, uint32_t amount)
{
    Item *item = FindItem(itemId);
    if (!item)
    {
        assert(false);
        return {};
    }

    InventoryModify modify{ *this };
    modify.IncrementItemAttribute(*item, AttributeDefIndex::KillEater, static_cast<int>(amount));
    return BuildChangeMessages(modify);
}

InventoryChangeMessages Inventory::NameItem(uint64_t nameTagId, uint64_t itemId, std::string_view name)
{
    InventoryModify modify{ *this };
    Item *item = FindItem(itemId);
    if (!item)
    {
        assert(false);
        return {};
    }

    modify.SetItemAttribute(*item, AttributeDefIndex::CustomName, std::string{ name });

    // caskets get updated here...
    if (item->DefIndex() == ItemDefIndex::Casket)
    {
        if (!item->HasAttribute(AttributeDefIndex::CasketItemsCount))
        {
            modify.SetItemAttribute(*item, AttributeDefIndex::CasketItemsCount, 0u);
        }

        uint32_t modifyTime = static_cast<uint32_t>(time(nullptr));
        modify.SetItemAttribute(*item, AttributeDefIndex::CasketModificationDate, modifyTime);
    }

    if (GetConfig().DestroyUsedItems())
    {
        assert(nameTagId != itemId);
        DestroyItemById(modify, nameTagId);
    }

    return BuildChangeMessages(modify, k_EGCItemCustomizationNotification_NameItem, { itemId });
}

InventoryChangeMessages Inventory::NameBaseItem(uint64_t nameTagId, uint32_t defIndex, std::string_view name)
{
    InventoryModify modify{ *this };

    ItemDesc desc{};
    m_itemSchema.GetItemDesc(ToEnum<ItemDefIndex>(defIndex), ItemOriginBaseItem, UnacknowledgedInvalid, desc);

    Item &item = CreateItem(modify, desc);
    uint64_t itemId = item.FullIdFor(AccountId());

    modify.SetItemAttribute(item, AttributeDefIndex::CustomName, std::string{ name });

    if (GetConfig().DestroyUsedItems())
    {
        assert(nameTagId != itemId);
        DestroyItemById(modify, nameTagId);
    }

    // mikkotodo def index???
    return BuildChangeMessages(modify, k_EGCItemCustomizationNotification_NameBaseItem, { itemId });
}

InventoryChangeMessages Inventory::RemoveItemName(uint64_t itemId)
{
    InventoryModify modify{ *this };
    Item *item = FindItem(itemId);
    if (!item)
    {
        assert(false);
        return {};
    }

    if (item->GetRarity() == Rarity::Default)
    {
        uint64_t fakeItemId = static_cast<uint64_t>(item->DefIndex()) | ItemIdDefaultItemMask;
        DestroyItem(modify, item);
        return BuildChangeMessages(modify, k_EGCItemCustomizationNotification_RemoveItemName, { fakeItemId });
    }

    modify.RemoveItemAttributes(*item, { AttributeDefIndex::CustomName });

    return BuildChangeMessages(modify, k_EGCItemCustomizationNotification_RemoveItemName, { item->FullIdFor(AccountId()) });
}

Item *Inventory::FindItem(uint64_t itemId)
{
    auto it = m_items.find(itemId);
    if (it != m_items.end())
    {
        return &it->second;
    }

    return nullptr;
}

static void EmbedStorageReference(InventoryModify &modify, Item &item, uint64_t storageId)
{
    uint32_t low = (storageId & UINT32_MAX);
    modify.SetItemAttribute(item, AttributeDefIndex::CasketIdLow, low);

    uint32_t high = (storageId >> 32) & UINT32_MAX;
    modify.SetItemAttribute(item, AttributeDefIndex::CasketIdHigh, high);

    modify.RemoveItemEquips(item);
}

InventoryChangeMessages Inventory::CasketItemAdd(uint64_t casketId, uint64_t itemId)
{
    InventoryModify modify{ *this };
    Item *storage = FindItem(casketId);
    if (!storage)
    {
        assert(false);
        return {};
    }

    Item *target = FindItem(itemId);
    if (!target)
    {
        assert(false);
        return {};
    }

    if (storage->DefIndex() != ItemDefIndex::Casket)
    {
        assert(false);
        return {};
    }

    auto result = modify.IncrementItemAttribute(*storage, AttributeDefIndex::CasketItemsCount, 1, 1000);
    if (result == AttributeIncrement::OutOfRange)
    {
        return BuildChangeMessages(modify, k_EGCItemCustomizationNotification_CasketTooFull, { casketId });
    }

    if (result != AttributeIncrement::Ok)
    {
        assert(false);
        return {};
    }

    uint32_t modifyTime = static_cast<uint32_t>(time(nullptr));
    modify.SetItemAttribute(*storage, AttributeDefIndex::CasketModificationDate, modifyTime);

    EmbedStorageReference(modify, *target, casketId);

    return BuildChangeMessages(modify, k_EGCItemCustomizationNotification_CasketAdded, { casketId });
}

InventoryChangeMessages Inventory::CasketItemExtract(uint64_t casketId, uint64_t itemId)
{
    InventoryModify modify{ *this };
    Item *storage = FindItem(casketId);
    if (!storage)
    {
        assert(false);
        return {};
    }

    Item *target = FindItem(itemId);
    if (!target)
    {
        assert(false);
        return {};
    }

    if (storage->DefIndex() != ItemDefIndex::Casket)
    {
        assert(false);
        return {};
    }

    auto result = modify.IncrementItemAttribute(*storage, AttributeDefIndex::CasketItemsCount, -1, 1000);
    if (result != AttributeIncrement::Ok)
    {
        assert(false);
        return {};
    }

    uint32_t modifyTime = static_cast<uint32_t>(time(nullptr));
    modify.SetItemAttribute(*storage, AttributeDefIndex::CasketModificationDate, modifyTime);

    modify.RemoveItemAttributes(*target, { AttributeDefIndex::CasketIdLow, AttributeDefIndex::CasketIdHigh });

    return BuildChangeMessages(modify, k_EGCItemCustomizationNotification_CasketRemoved, { casketId });
}

InventoryChangeMessages Inventory::PurchaseItems(const std::vector<uint32_t> &defIndexes, std::vector<uint64_t> &itemIds)
{
    itemIds.clear();
    itemIds.reserve(defIndexes.size());

    InventoryModify modify{ *this };

    for (uint32_t defIndex : defIndexes)
    {
        ItemDesc desc{};
        m_itemSchema.GetItemDesc(ToEnum<ItemDefIndex>(defIndex), ItemOriginPurchased, UnacknowledgedPurchased, desc);
        Item &item = CreateItem(modify, desc);
        itemIds.push_back(item.FullIdFor(AccountId()));
    }

    return BuildChangeMessages(modify);
}

bool Inventory::UnequipItem(InventoryModify &modify, uint64_t itemId)
{
    uint32_t defIndex, paintKitIndex;
    if (IsDefaultItemId(itemId, defIndex, paintKitIndex))
    {
        // not supported
        assert(false);
        return false;
    }

    Item *item = FindItem(itemId);
    if (!item)
    {
        assert(false);
        return false;
    }

    modify.RemoveItemEquips(*item);

    return true;
}

// this goes through everything on purpose
void Inventory::UnequipItem(InventoryModify &modify, uint32_t classId, uint32_t slotId)
{
    // check non default items first
    for (auto &pair : m_items)
    {
        if (modify.RemoveItemEquip(pair.second, classId, slotId))
        {
            Platform::Print("Unequip {} class {} slot {}\n", pair.first, classId, slotId);
        }
    }

    // check default equips, just delete the references locally
    for (auto it = m_defaultEquips.begin(); it != m_defaultEquips.end();)
    {
        if (it->classId == classId && it->slotId == slotId)
        {
            Platform::Print("Unequip {} class {} slot {}\n", it->defIndex, classId, slotId);
            it = m_defaultEquips.erase(it);
        }
        else
        {
            it++;
        }
    }
}

bool Inventory::DestroyItemById(InventoryModify &modify, uint64_t itemId)
{
    auto it = m_items.find(itemId);
    if (it == m_items.end())
    {
        return false;
    }

    Item &item = it->second;

    // schizo: if this item was in a casket, decrement the casket item count
    Item *casket = FindItem(GetCasketId(item));
    if (casket && casket->DefIndex() == ItemDefIndex::Casket)
    {
        modify.IncrementItemAttribute(*casket, AttributeDefIndex::CasketItemsCount, -1);
        modify.SetItemAttribute(*casket, AttributeDefIndex::CasketModificationDate, static_cast<uint32_t>(time(nullptr)));
    }

    // schizo: if this was a casket, take all of the items out
    if (item.DefIndex() == ItemDefIndex::Casket)
    {
        for (auto &pair : m_items)
        {
            if (GetCasketId(pair.second) == itemId)
            {
                modify.RemoveItemAttributes(pair.second, { AttributeDefIndex::CasketIdLow, AttributeDefIndex::CasketIdHigh });
            }
        }
    }

    bool hadEquips = item.HasEquips();
    m_items.erase(it);

    modify.MarkItemDestroyed(itemId >> 32, hadEquips);
    return true;
}

void Inventory::DestroyItem(InventoryModify &modify, Item *item)
{
    bool destroyed = DestroyItemById(modify, item->FullIdFor(AccountId()));
    if (!destroyed)
    {
        // how is this possible???
        assert(false);
    }
}
