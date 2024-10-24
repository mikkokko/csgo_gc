#pragma once

#include <steam/isteamnetworkingmessages.h>

constexpr int NetMessageSendFlags = k_nSteamNetworkingSend_Reliable;
constexpr int NetMessageChannel = 7;

// NOTE: these are used as gc message types! 
// if they overlap with the game's gc messages, we're doomed
enum ENetworkMsg : uint32_t
{
    // sent by the server to client when they connect, data is the auth ticket
    k_EMsgNetworkConnect = (1u << 31) - 1,
};
