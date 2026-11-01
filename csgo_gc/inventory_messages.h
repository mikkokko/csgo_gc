#pragma once

#include "gcsdk_gcmessages.pb.h"
#include "econ_gcmessages.pb.h"

struct SOCacheMessages
{
    std::vector<CMsgSOSingleObject> created;
    std::vector<CMsgSOSingleObject> destroyed;
    CMsgSOMultipleObjects updated;
};

struct InventoryChangeMessages
{
    SOCacheMessages client;
    SOCacheMessages gameServer;

    // client only
    CMsgGCItemCustomizationNotification notification;
};
