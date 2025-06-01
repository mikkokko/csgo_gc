#include "stdafx.h"
#include "gc_message.h"

GCMessageRead::GCMessageRead(uint32_t type, const void *data, uint32_t size)
    : m_data{ static_cast<const uint8_t *>(data) }
    , m_size{ size }
{
    m_type = ReadUint32();
    if (!IsValid())
    {
        assert(false);
        return;
    }

    // override the type if provided
    if (type)
    {
        m_type = type;
    }

    if (IsProtobuf())
    {
        // reading a ProtoMsgHeader
        uint32_t headerSize = ReadUint32();
        if (headerSize)
        {
            CMsgProtoBufHeader header;
            const void *headerData = ReadData(headerSize);
            if (!header.ParseFromArray(headerData, headerSize))
            {
                assert(false);
                m_error = true;
                return;
            }

            assert(header.client_steam_id() == 0);
            assert(header.client_session_id() == 0);
            assert(header.source_app_id() == 0);
            assert(header.job_id_source() != JobIdInvalid);
            assert(header.job_id_target() == JobIdInvalid);
            assert(header.target_job_name() == "");
            assert(header.eresult() == 2);
            assert(header.error_message() == "");
            assert(header.ip() == 0);
            assert(header.gc_msg_src() == 0);
            assert(header.gc_dir_index_source() == 0);

            m_jobId = header.job_id_source();
        }
    }
    else
    {
        // reading a GameStructMsgHeader
        ReadUint32();
        ReadUint64();
        ReadUint16();
    }

    // caller needs to check for this
    assert(IsValid());
}

const void *GCMessageRead::ReadData(size_t size)
{
    if (m_error)
    {
        // shouldn't get called
        assert(false);
        return nullptr;
    }

    if (m_offset + size > m_size)
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

// mikkotodo fix!!! this function is fucked and broken
std::string_view GCMessageRead::ReadString()
{
    if (m_error)
    {
        // shouldn't get called
        assert(false);
        return {};
    }

    for (uint32_t i = m_offset; i < m_size; i++)
    {
        if (m_data[i] == '\0')
        {
            std::string_view result{ reinterpret_cast<const char *>(&m_data[m_offset]), i - m_offset };
            m_offset += result.size() + 1;
            return result;
        }
    }

    // overflow
    assert(false);
    m_error = true;
    return {};
}

// mikkotodo useful elswhere as well???
static void AppendProtobuf(std::vector<uint8_t> &buffer, const google::protobuf::MessageLite &message)
{
    size_t protobufOffset = buffer.size();
    size_t protobufSize = message.ByteSizeLong();
    buffer.resize(buffer.size() + protobufSize);

    [[maybe_unused]] bool result = message.SerializeToArray(buffer.data() + protobufOffset, protobufSize);
    assert(result);
}

GCMessageWrite::GCMessageWrite(uint32_t type, const google::protobuf::MessageLite &message, uint64_t jobId)
    : m_type{ type | ProtobufMask }
{
    // write the protobuf message hader
    WriteUint32(m_type);

    if (jobId != JobIdInvalid)
    {
        // response to a job
        CMsgProtoBufHeader header;
        header.set_job_id_target(jobId);

        // write the header size and the data
        WriteUint32(header.ByteSizeLong());
        AppendProtobuf(m_buffer, header);
    }
    else
    {
        // no need for a CMsgProtoBufHeader
        WriteUint32(0);
    }

    // append the serialized protobuf message
    AppendProtobuf(m_buffer, message);
}

GCMessageWrite::GCMessageWrite(uint32_t type)
    : m_type{ type }
{
    // write the non protobuf messge hader
    // mikkotoodo using GameStructMsgHeader is wrong here!!! we should be using the fat one
    // however we're not sending these to the game (yet) so it doesn't matter
    WriteUint32(m_type);
    WriteUint32(0);
    WriteUint64(0);
    WriteUint16(0);
}

GCMessageWrite::GCMessageWrite(const void *data, uint32_t size)
    : m_type{ 0 } // don't know yet
{
    if (size >= sizeof(uint32_t))
    {
        m_type = *reinterpret_cast<const uint32_t *>(data);
    }
    else
    {
        assert(false);
    }

    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(data);
    m_buffer.assign(bytes, bytes + size);
}

void GCMessageWrite::WriteData(const void *data, uint32_t size)
{
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(data);
    m_buffer.insert(m_buffer.end(), bytes, bytes + size);
}
