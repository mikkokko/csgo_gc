#include "stdafx.h"
#include "platform.h"
#include "steam_hook.h"

#if defined(_MSC_VER)
#define DLL_EXPORT extern "C" __declspec(dllexport)
#elif defined(__GNUC__)
#define DLL_EXPORT extern "C" __attribute__((visibility("default")))
#else
#error
#endif

DLL_EXPORT void InstallGC(bool dedicated)
{
    Platform::Initialize();
    SteamHookInstall(dedicated);
}
