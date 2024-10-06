#pragma once

class Random
{
public:
    uint32_t Uint32()
    {
        return std::uniform_int_distribution<uint32_t>{}(m_engine);
    }

private:
    std::mt19937 m_engine{ std::random_device{}() };
};
