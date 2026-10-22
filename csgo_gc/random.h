#pragma once

namespace Random
{

uint32_t Uint32(uint32_t min = 0, uint32_t max = UINT32_MAX);
float Float(float min = 0.0f, float max = 1.0f);

template<typename T>
T &Select(std::span<T> elements)
{
    assert(!elements.empty() && elements.size() <= UINT32_MAX);
    return elements[Uint32(0, static_cast<uint32_t>(elements.size() - 1))];
}

}
