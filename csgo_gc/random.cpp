#include "stdafx.h"
#include "random.h"

namespace Random
{

static thread_local std::minstd_rand s_rng{ std::random_device{}() };

uint32_t Uint32(uint32_t min, uint32_t max)
{
    return std::uniform_int_distribution<uint32_t>{ min, max }(s_rng);
}

float Float(float min, float max)
{
    return std::uniform_real_distribution<float>{ min, max }(s_rng);
}

}
