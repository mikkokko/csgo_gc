#pragma once

namespace Random
{

uint32_t Uint32(uint32_t min = 0, uint32_t max = UINT32_MAX);
float Float(float min = 0.0f, float max = 1.0f);

template<typename C>
auto &Select(const C &elements)
{
    assert(!std::empty(elements) && std::size(elements) <= UINT32_MAX);
    return elements[Uint32(0, static_cast<uint32_t>(std::size(elements) - 1))];
}

}
