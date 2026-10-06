#pragma once

#include <assert.h>

#include <array>
#include <charconv>
#include <condition_variable>
#include <deque>
#include <format>
#include <list>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

template<typename Enum>
    requires std::is_enum_v<Enum>
[[nodiscard]] constexpr std::underlying_type_t<Enum> FromEnum(Enum value) noexcept
{
    return static_cast<std::underlying_type_t<Enum>>(value);
}

template<typename Enum>
    requires std::is_enum_v<Enum>
[[nodiscard]] constexpr Enum ToEnum(std::underlying_type_t<Enum> value) noexcept
{
    return static_cast<Enum>(value);
}

#include "base_gcmessages.pb.h"
#include "cstrike15_gcmessages.pb.h"
#include "econ_gcmessages.pb.h"
#include "engine_gcmessages.pb.h"
#include "gcsdk_gcmessages.pb.h"
#include "gcsystemmsgs.pb.h"
#include "steammessages.pb.h"

// used in many files for logging
#include "platform.h"

// the struct name should make it obvious what it does
template<class... T>
struct Bruh : T...
{
    using T::operator()...;
};
