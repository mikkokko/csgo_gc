#pragma once

#include <steam/isteamnetworkingmessages.h>

constexpr int NetMessageSendFlags = k_nSteamNetworkingSend_Reliable;
constexpr int NetMessageChannel = 7;

// NOTE: these are used as gc message types!
// if they overlap with the game's gc messages, we're doomed
enum ENetworkMsg : uint32_t
{
    k_EMsgNetworkFirst = (1u << 31) - 16,

    // sent by the server to client when they connect, data is the auth ticket
    k_EMsgNetworkConnect = (1u << 31) - 1,

    k_EMsgNetworkLast = k_EMsgNetworkConnect
};

inline bool IsNetworkMessage(uint32_t type)
{
    return type >= k_EMsgNetworkFirst && type <= k_EMsgNetworkLast;
}
