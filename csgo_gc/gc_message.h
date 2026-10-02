#pragma once

#include "gc_const.h"
#include "message.h"

class GCMessageRead : public MessageRead
{
public:
    GCMessageRead(uint32_t type, const void *data, uint32_t size);

    std::string_view ReadString(); // creepy shit

    bool IsProtobuf() const { return m_type & ProtobufMask; }
    uint32_t TypeUnmasked() const { return m_type & ~ProtobufMask; }
    uint32_t TypeMasked() const { return m_type; }
    uint64_t JobId() const { return m_jobId; }

    template<typename T>
    bool ReadProtobuf(T &message)
    {
        assert(IsProtobuf());

        // read the remainder as a protobuf message
        uint32_t size = m_size - m_offset;
        const void *data = ReadData(size);
        if (!data)
        {
            assert(false);
            return false;
        }

        return message.ParseFromArray(data, size);
    }

private:
    uint32_t m_type; // parsed from the message, protobuf mask is kept
    uint64_t m_jobId{ JobIdInvalid };
};

class GCMessageWrite : public MessageWrite
{
public:
    // protobuf messages
    GCMessageWrite(uint32_t type, const google::protobuf::MessageLite &message, uint64_t jobId = JobIdInvalid);

    // non protobuf messages, data written with the writer functions
    GCMessageWrite(uint32_t type);

    // already serialized data that just gets copied over, type parsed from the message
    GCMessageWrite(const void *data, uint32_t size);

    GCMessageWrite(GCMessageWrite &&) = default;
    GCMessageWrite &operator=(GCMessageWrite &&) = default;

    // shouldn't have to copy these
    GCMessageWrite(const GCMessageWrite &) = delete;
    GCMessageWrite &operator=(const GCMessageWrite &) = delete;

    uint32_t TypeMasked() const
    {
        if (m_buffer.size() < sizeof(uint32_t))
        {
            assert(false);
            return 0;
        }

        uint32_t type = *reinterpret_cast<const uint32_t *>(m_buffer.data());
        assert(type);
        return type;
    }
};
