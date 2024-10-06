#pragma once

// game independent gc constants

constexpr uint32_t ProtobufMask = 0x80000000;

// CMsgSOIDOwner type
enum SoIdType
{
    SoIdTypeSteamId = 1
};

// header for protobuf messages, used by both gc and game
struct ProtoMsgHeader
{
    uint32_t type;
    uint32_t headerSize;
};
