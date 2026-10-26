#include "stdafx.h"
#include "gc_message.h"
#include "networking_shared.h"

#include "steammessages.pb.h"

inline bool IsProtobufMessage(uint32_t type)
{
    return type & ProtobufMask;
}

GCMessageRead::GCMessageRead(uint32_t type, const void *data, uint32_t size)
    : MessageRead{ data, size }
{
    // net messages and protobuf messages write the type on wire
    if (!type || IsProtobufMessage(type) || IsNetworkMessage(type))
    {
        uint32 headerType = ReadUint32();
        if (!IsValid() || (!IsProtobufMessage(headerType) && !IsNetworkMessage(headerType)))
        {
            assert(false);
            m_error = true;
            return;
        }

        assert(headerType);
        assert(!type || headerType == type);
        m_type = type ? type : headerType;
    }
    else
    {
        m_type = type;
    }

    if (IsNetworkMessage(m_type))
    {
        // csgo_gc legacy trauma header
        ReadData(14);
    }
    else if (IsProtobuf())
    {
        // reading a ProtoMsgHeader
        uint32_t headerSize = ReadUint32();
        if (headerSize)
        {
            CMsgProtoBufHeader header;
            const void *headerData = ReadData(headerSize);
            if (!headerData || !header.ParseFromArray(headerData, headerSize))
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
        // reading a StructMsgHeader
        uint16_t version = ReadUint16();
        uint64_t jobIdTarget = ReadUint64();
        uint64_t jobIdSource = ReadUint64();

        assert(version == 1);
        assert(jobIdTarget == JobIdInvalid);
        m_jobId = jobIdSource;
    }

    // caller needs to check for this
    assert(IsValid());
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

GCMessageWrite::GCMessageWrite(uint32_t type, uint64_t jobId)
    : m_type{type}
{
    if (IsNetworkMessage(type))
    {
        // csgo_gc legacy trauma header
        WriteUint32(type);
        WriteUint32(0);
        WriteUint64(0);
        WriteUint16(0);
    }
    else
    {
        WriteUint16(1);
        WriteUint64(jobId);
        WriteUint64(JobIdInvalid);
    }
}
