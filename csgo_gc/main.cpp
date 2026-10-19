#include "stdafx.h"
#include "platform.h"
#include "ping_service.h"
#include "steam_hook.h"

void InstallGC(bool dedicated)
{
    Platform::Initialize();
    SteamHookInstall(dedicated);

    if (!dedicated)
    {
        PingServiceStart();
    }
}
