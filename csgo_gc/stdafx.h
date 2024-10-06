#pragma once

#include <assert.h>

#include <charconv>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

// might as well
#include "base_gcmessages.pb.h"
#include "cstrike15_gcmessages.pb.h"
#include "econ_gcmessages.pb.h"
#include "engine_gcmessages.pb.h"
#include "gcsdk_gcmessages.pb.h"
#include "gcsystemmsgs.pb.h"

// debug session
// mikkotodo wtf is the comment above and why is this here
namespace Platform
{
void Print(const char *format, ...);
}
