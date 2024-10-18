#pragma once

class Random
{
public:
    uint32_t Uint32(uint32_t min = 0, uint32_t max = UINT32_MAX)
    {
        return std::uniform_int_distribution<uint32_t>{ min, max }(m_engine);
    }

    float Float(float min = 0.0f, float max = 1.0f)
    {
        return std::uniform_real_distribution<float>{ min, max }(m_engine);
    }

    size_t RandomIndex(size_t size)
    {
        assert(size);
        return std::uniform_int_distribution<size_t>{ 0, size - 1 }(m_engine);
    }

private:
    std::mt19937 m_engine{ std::random_device{}() };
};

extern Random g_random;
