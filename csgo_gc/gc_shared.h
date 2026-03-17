#pragma once

#include "gc_message.h"

enum class HostEvent
{
    Message, // id contains the message type, buffer contains the payload
    NetMessage, // id contains the recipient steam id, buffer contains the payload
    MicroTransactionResponse, // runs MicroTxnAuthorizationResponse_t, no arguments
};

enum class GCEvent
{
    Message, // id contains the message type, buffer contains the payload
    NetMessage, // id contains the recipient steam id, buffer contains the payload
    SOCacheRequest, // sent to client gc when connected to a gameserver
    ClientSOCacheUnsubscribe, // sent to server gc when a client disconnects, id contains the steam id
};

struct EventData
{
    int type; // HostEvent or GCEvent
    uint64_t id;
    std::vector<uint8_t> buffer;
};

// shared logic between client and server gcs
class SharedGC
{
public:
    void PostToGC(GCEvent type, uint64_t id, const void *data, uint32_t dataSize);

    void GetHostEvents(std::vector<EventData> &events);

protected:
    // must be manually called by the gc
    void StartThread();
    void StopThread();

    void PostToHost(HostEvent type, uint64_t id, const void *data, uint32_t dataSize);

private:
    virtual void HandleEvent(GCEvent type, uint64_t id, const std::vector<uint8_t> &buffer) = 0;

    void WorkerThread();

    std::mutex m_gcEventMutex;
    std::condition_variable m_cv;
    std::vector<EventData> m_gcEvents;
    std::thread m_thread;
    bool m_stopping{ false };

    std::mutex m_hostEventMutex;
    std::vector<EventData> m_hostEvents;
};

const char *MessageName(uint32_t type);
