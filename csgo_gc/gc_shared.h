#pragma once

#include "gc_message.h"

// only the message queue code is the same on client and server gcs
class SharedGC
{
public:
    bool HasOutgoingMessages(uint32_t &size);
    bool PopOutgoingMessage(uint32_t &type, void *buffer, uint32_t bufferSize, uint32_t &size);

protected: 
    std::queue<GCMessageWrite> m_outgoingMessages;
};
