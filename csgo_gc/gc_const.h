#pragma once

// game independent gc constants

constexpr uint32_t ProtobufMask = 0x80000000;

constexpr uint64_t JobIdInvalid = 0xffffffffffffffff;

// CMsgSOIDOwner type
enum SoIdType
{
    SoIdTypeSteamId = 1
};
