#pragma once

#include "gcsdk_gcmessages.pb.h"
#include "econ_gcmessages.pb.h"

struct SingleObject
{
    CMsgSOSingleObject proto;
    bool sendToGameServer;
};

// this should be revisited later... these semantics
// are not exactly correct, but it'll do for now
struct InventoryChangeMessages
{
    // full shared object cache update for clients
    CMsgSOMultipleObjects updatedClient;

    // janky... same as above but only contains equipped items
    CMsgSOMultipleObjects updatedGameServer;

    // client, or both client and game server
    std::vector<SingleObject> created;
    std::vector<SingleObject> destroyed;

    // client only
    CMsgGCItemCustomizationNotification notification;
};
