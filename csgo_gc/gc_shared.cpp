#include "stdafx.h"
#include "gc_shared.h"

const char *MessageName(uint32_t type);

bool SharedGC::HasOutgoingMessages(uint32_t &size)
{
    if (m_outgoingMessages.empty())
    {
        return false;
    }

    GCMessageWrite &message = m_outgoingMessages.front();
    size = message.Size();

    return true;
}

bool SharedGC::PopOutgoingMessage(uint32_t &type, void *buffer, uint32_t bufferSize, uint32_t &size)
{
    if (m_outgoingMessages.empty())
    {
        size = 0;
        return false;
    }

    GCMessageWrite &message = m_outgoingMessages.front();
    type = message.TypeMasked();
    size = message.Size();

    if (bufferSize < message.Size())
    {
        return false;
    }

    memcpy(buffer, message.Data(), message.Size());
    m_outgoingMessages.pop();
    return true;
}
