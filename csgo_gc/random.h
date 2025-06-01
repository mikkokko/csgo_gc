#pragma once

class Random
{
public:
    template<typename T>
    T Integer(T min, T max)
    {
        return std::uniform_int_distribution<T>{ min, max }(m_engine);
    }

    float Float(float min = 0.0f, float max = 1.0f)
    {
        return std::uniform_real_distribution<float>{ min, max }(m_engine);
    }

private:
    std::mt19937 m_engine{ std::random_device{}() };
};
