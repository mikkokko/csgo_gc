#pragma once

#include "gc_const.h"

class GCMessageRead
{
public:
    GCMessageRead(uint32_t type, const void *data, uint32_t size);

    const void *ReadData(size_t size);
    std::string_view ReadString(); // creepy shit

    bool IsValid() const { return !m_error; }
    bool IsProtobuf() const { return m_type & ProtobufMask; }
    uint32_t TypeUnmasked() const { return m_type & ~ProtobufMask; }
    uint64_t JobId() const { return m_jobId; }

    template<typename T>
    bool ReadProtobuf(T &message)
    {
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

    // ReadData wrappers
    template<typename T>
    T ReadVariable()
    {
        const T *variable = static_cast<const T *>(ReadData(sizeof(T)));
        if (!variable)
        {
            assert(false);
            return 0;
        }

        return *variable;
    }

    uint16_t ReadUint16() { return ReadVariable<uint16_t>(); }
    uint32_t ReadUint32() { return ReadVariable<uint32_t>(); }
    uint64_t ReadUint64() { return ReadVariable<uint64_t>(); }

private:
    const uint8_t *const m_data;
    const uint32_t m_size;
    uint32_t m_type; // parsed from the message, protobuf mask is kept
    uint64_t m_jobId{ JobIdInvalid };

    // the state
    uint32_t m_offset{};
    bool m_error{};
};

class GCMessageWrite
{
public:
    // protobuf messages
    GCMessageWrite(uint32_t type, const google::protobuf::MessageLite &message, uint64_t jobId = JobIdInvalid);

    // non protobuf messages, data written with the writer functions
    GCMessageWrite(uint32_t type);

    // already serialized data that just gets copied over, type parsed from the message
    GCMessageWrite(const void *data, uint32_t size);

    // temp validation
    GCMessageWrite(const GCMessageWrite &) = delete;
    GCMessageWrite(GCMessageWrite &&) = delete;
    GCMessageWrite &operator=(const GCMessageWrite &) = delete;
    GCMessageWrite &operator=(GCMessageWrite &&) = delete;

    // non protobuf message writing
    void WriteData(const void *data, uint32_t size);

    uint32_t TypeMasked() const { return m_type; }
    const void *Data() const { return m_buffer.data(); }
    uint32_t Size() const { return m_buffer.size(); }

    // writing helpers
    void WriteUint16(uint16_t value) { WriteData(&value, sizeof(value)); }
    void WriteUint32(uint32_t value) { WriteData(&value, sizeof(value)); }
    void WriteUint64(uint64_t value) { WriteData(&value, sizeof(value)); }

private:
    uint32_t m_type;
    std::vector<uint8_t> m_buffer;
};
