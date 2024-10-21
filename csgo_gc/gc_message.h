#pragma once

#include "gc_const.h"

// mikkotodo all of this sucks

// mikkotodo error checking...
template<typename T>
inline bool ReadGCMessage(T &message, const void *data, uint32_t size)
{
    const ProtoMsgHeader *header = reinterpret_cast<const ProtoMsgHeader *>(data);

    // should have been checked before calling
    assert(header->type & ProtobufMask);

    const void *protoData = reinterpret_cast<const uint8_t *>(data) + sizeof(*header) + header->headerSize;
    uint32_t protoSize = size - sizeof(*header) - header->headerSize;

    return message.ParseFromArray(protoData, protoSize);
}

// this also sucks
template<typename T, bool ExtraData = false>
const T *ReadGameStructMessage(const void *data, uint32_t size)
{
    if (!ExtraData && size != sizeof(GameStructMsgHeader) + sizeof(T))
    {
        assert(false);
        return nullptr;
    }

    if (ExtraData && size <= sizeof(GameStructMsgHeader) + sizeof(T))
    {
        assert(false);
        return nullptr;
    }

    const uint8_t *messageData = reinterpret_cast<const uint8_t *>(data);
    return reinterpret_cast<const T *>(messageData + sizeof(GameStructMsgHeader));
}

inline void AppendGCMessage(std::vector<uint8_t> &buffer, uint32_t type, const google::protobuf::MessageLite &message)
{
    assert(type & ProtobufMask);

    // mikkotodo inefficent
    size_t currentSize = buffer.size();
    buffer.resize(currentSize + sizeof(ProtoMsgHeader) + message.ByteSizeLong());

    ProtoMsgHeader *pHeader = reinterpret_cast<ProtoMsgHeader *>(buffer.data() + currentSize);
    pHeader->type = type;
    pHeader->headerSize = 0;

    message.SerializeToArray(buffer.data() + currentSize + sizeof(ProtoMsgHeader), message.ByteSizeLong());
}

// helper class for gc --> game messages
class OutgoingMessage
{
public:
    // protobuf message constructor
    OutgoingMessage(uint32_t type, const google::protobuf::MessageLite &message)
        : m_type{ type | ProtobufMask }
    {
        AppendGCMessage(m_buffer, m_type, message);
    }

    // already serialized data constructor
    OutgoingMessage(const void *data, uint32_t size)
        : m_type{ *reinterpret_cast<const uint32_t *>(data) } // mikkotodo unsafe
    {
        m_buffer.resize(size);
        memcpy(m_buffer.data(), data, size);
    }

    uint32_t Type() const { return m_type; }
    const void *Data() const { return m_buffer.data(); }
    uint32_t Size() const { return static_cast<uint32_t>(m_buffer.size()); }

private:
    const uint32_t m_type;
    std::vector<uint8_t> m_buffer;
};
