#pragma once

#include "item.h"

class ItemSchema;

struct lws;
struct lws_context;

enum class EditorItemChangeType
{
    Created,
    Modified,
    Destroyed
};

struct EditorItem
{
    EditorItemChangeType type;
    uint32_t highId;
    ItemDesc desc;
};

struct EditorChanges
{
    std::vector<EditorItem> items;
};

// a websocket server an external inventory editor can tap into
class InventoryEditor
{
public:
    InventoryEditor(ItemSchema &itemSchema);
    ~InventoryEditor();

    // update connection state, and get changes made by the inventory
    // returns false if no changes have been made by them
    bool GetChanges(EditorChanges &changes);

    // let the editor know about changes that have been made by us
    void SendChanges(const EditorChanges &changes, bool isFull);

    // let the editor know about an id we decided on for this item
    void SendItemId(uint32_t requestId, uint32_t highId);

    // indicates that the whole inventory should be sent
    bool WantsFullInventory() const { return m_wantsFullInventory; }

private:
    using Message = std::vector<uint8_t>;

    struct Session
    {
        // we might not receive all data at once
        Message partial;
    };

    static int Callback(lws *wsi, int reason, void *user, void *in, size_t len);

    bool ParseMessage(const Message &message, EditorChanges &changes);
    bool SerializeMessage(const EditorChanges &changes, Message &message, bool isFull);

    // for attribute conversion...
    ItemSchema &m_itemSchema;

    lws_context *m_context{};
    lws *m_client{};
    std::deque<Message> m_inbox;
    std::deque<Message> m_outbox;

    bool m_wantsFullInventory{};
};
