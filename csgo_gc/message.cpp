#include "stdafx.h"
#include "message.h"

MessageRead::MessageRead(const void *data, uint32_t size)
    : m_data{ static_cast<const uint8_t *>(data) }
    , m_size{ size }
{
}

const void *MessageRead::ReadData(size_t size)
{
    if (m_error)
    {
        // shouldn't get called
        assert(false);
        return nullptr;
    }

    if (size > m_size - m_offset)
    {
        // overflow
        assert(false);
        m_error = true;
        return nullptr;
    }

    const void *result = &m_data[m_offset];
    m_offset += size;
    return result;
}

void MessageWrite::WriteData(const void *data, uint32_t size)
{
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(data);
    m_buffer.insert(m_buffer.end(), bytes, bytes + size);
}
