#pragma once

class MessageRead
{
public:
    MessageRead(const void *data, uint32_t size);

    const void *ReadData(size_t size);

    bool IsValid() const { return !m_error; }
    bool IsDone() const { return m_offset >= m_size; }

    // ReadData wrappers
    template<typename T>
    T ReadVariable()
    {
        const void *address = ReadData(sizeof(T));
        if (!address)
        {
            assert(false);
            return 0;
        }

        T variable;
        memcpy(&variable, address, sizeof(T));
        return variable;
    }

    uint16_t ReadUint16() { return ReadVariable<uint16_t>(); }
    uint32_t ReadUint32() { return ReadVariable<uint32_t>(); }
    uint64_t ReadUint64() { return ReadVariable<uint64_t>(); }

protected:
    const uint8_t *const m_data;
    const uint32_t m_size;

    // the state
    uint32_t m_offset{};
    bool m_error{};
};

class MessageWrite
{
public:
    MessageWrite() = default;

    MessageWrite(MessageWrite &&) = default;
    MessageWrite &operator=(MessageWrite &&) = default;

    // shouldn't have to copy these
    MessageWrite(const MessageWrite &) = delete;
    MessageWrite &operator=(const MessageWrite &) = delete;

    void WriteData(const void *data, uint32_t size);

    const void *Data() const { return m_buffer.data(); }
    uint32_t Size() const { return m_buffer.size(); }

    // writing helpers
    void WriteUint16(uint16_t value) { WriteData(&value, sizeof(value)); }
    void WriteUint32(uint32_t value) { WriteData(&value, sizeof(value)); }
    void WriteUint64(uint64_t value) { WriteData(&value, sizeof(value)); }

    std::vector<uint8_t> TakeBuffer() &&
    {
        return std::move(m_buffer);
    }

protected:
    std::vector<uint8_t> m_buffer;
};
