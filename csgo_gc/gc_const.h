#pragma once

// game independent gc constants

constexpr uint32_t ProtobufMask = 0x80000000;

// CMsgSOIDOwner type
enum SoIdType
{
    SoIdTypeSteamId = 1
};

#pragma pack(push, 1)

// header for protobuf messages, used by both gc and game
struct ProtoMsgHeader
{
    uint32_t type;
    uint32_t headerSize;
};

// header for struct based messages sent by the game
struct GameStructMsgHeader
{
	uint32_t type;
	uint32_t unused1;
	uint64_t unused2;
	uint16_t unused3;
};

#pragma pack(pop)
