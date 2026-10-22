#include "stdafx.h"
#include "platform.h"
#include "config.h"
#include "steam_hook.h"

void InstallGC(bool dedicated)
{
    LoadConfig();
    Platform::Initialize();
    SteamHookInstall(dedicated);
}
