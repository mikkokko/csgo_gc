#pragma once

#include <steam/steamnetworkingtypes.h>

constexpr uint16_t NetMessageSentinel = 0xC56C;

enum NetMessageType : uint16_t
{
    NetMessageInvalid,
    NetMessageConnected,
    NetMessageDisconnected,
    NetMessageForGame,
    NetMessageForGC,
    NetMessageLast = NetMessageForGC
};

struct NetMessageHeader
{
    uint16_t sentinel;
    NetMessageType type;
};

constexpr int MessageSendFlags = k_nSteamNetworkingSend_Reliable;
constexpr int MessageChannel = 8;

struct GCMessage
{
    const void *data;
    uint32_t size;
};

// helper
inline NetMessageType ParseNetMessage(const void *data, uint32_t size, GCMessage &gcMessage)
{
    // validate the size
    if (size < sizeof(NetMessageHeader))
    {
        return NetMessageInvalid;
    }

    // validate the header
    const NetMessageHeader *header = reinterpret_cast<const NetMessageHeader *>(data);
    if (header->sentinel != NetMessageSentinel)
    {
        return NetMessageInvalid;
    }

    if (header->type <= NetMessageInvalid || header->type > NetMessageLast)
    {
        return NetMessageInvalid;
    }

    // fill the gc message info, if any
    if (header->type == NetMessageForGame || header->type == NetMessageForGC)
    {
        // we could do more validation here but i'm lazy,
        // the game will check the messages anyway
        gcMessage.data = header + 1;
        gcMessage.size = size - sizeof(NetMessageHeader);
    }

    return header->type;
}
