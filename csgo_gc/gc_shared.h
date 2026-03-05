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

// shared logic between client and server gcs
class SharedGC
{
public:
    void PostToGC(GCEvent type, uint64_t id, const void *data, uint32_t dataSize);

    bool PollFromGC(HostEvent &type, uint64_t &id, std::vector<uint8_t> &buffer);

protected:
    // must be manually called by the gc
    void StartThread();
    void StopThread();

    void PostToHost(HostEvent type, uint64_t id, const void *data, uint32_t dataSize);

private:
    virtual void HandleEvent(GCEvent type, uint64_t id, const std::vector<uint8_t> &buffer) = 0;

    void WorkerThread();

    struct Event
    {
        int type; // HostEvent or GCEvent
        uint64_t id;
        std::vector<uint8_t> buffer;
    };

    std::mutex m_gcEventMutex;
    std::condition_variable m_cv;
    std::queue<Event> m_gcEvents;
    std::thread m_thread;
    bool m_stopping{ false };

    std::mutex m_hostEventMutex;
    std::queue<Event> m_hostEvents;
};

const char *MessageName(uint32_t type);
