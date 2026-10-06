#include "stdafx.h"
#include "gc_shared.h"

void SharedGC::StartThread()
{
    m_thread = std::thread{ &SharedGC::WorkerThread, this };
}

void SharedGC::StopThread()
{
    {
        std::lock_guard lock{ m_gcEventMutex };
        m_stopping = true;
    }

    m_cv.notify_one();
    m_thread.join();
}

void SharedGC::WorkerThread()
{
    std::vector<EventData> events;

    while (true)
    {
        {
            std::unique_lock lock{ m_gcEventMutex };

            // sleep until we get an event, or are shutting down
            m_cv.wait(lock, [this]
                { return !m_gcEvents.empty() || m_stopping; });

            if (m_stopping)
            {
                break;
            }

            std::swap(events, m_gcEvents);
        }

        for (EventData &event : events)
        {
            HandleEvent(static_cast<GCEvent>(event.type), event.id, event.buffer);
        }

        events.clear();
    }
}

void SharedGC::GetHostEvents(std::vector<EventData> &events)
{
    std::lock_guard lock{ m_hostEventMutex };
    assert(events.empty());
    std::swap(events, m_hostEvents);
}

void SharedGC::PostToGC(GCEvent type, uint64_t id, const void *data, uint32_t dataSize)
{
    const uint8_t *dataBegin = reinterpret_cast<const uint8_t *>(data);
    const uint8_t *dataEnd = dataBegin + dataSize;

    EventData event;
    event.type = static_cast<int>(type);
    event.id = id;
    event.buffer.assign(dataBegin, dataEnd);

    bool notify = false;
    {
        std::lock_guard lock{ m_gcEventMutex };
        notify = m_gcEvents.empty();
        m_gcEvents.push_back(std::move(event));
    }

    if (notify)
    {
        m_cv.notify_one();
    }
}

void SharedGC::PostToHost(HostEvent type, uint64_t id, std::vector<uint8_t> &&buffer)
{
    EventData event;
    event.type = static_cast<int>(type);
    event.id = id;
    event.buffer = std::move(buffer);

    {
        std::lock_guard lock{ m_hostEventMutex };
        m_hostEvents.push_back(std::move(event));
    }
}
