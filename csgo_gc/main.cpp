#include "stdafx.h"
#include "platform.h"
#include "config.h"
#include "steam_hook.h"

void InstallGC(bool dedicated)
{
    Platform::Initialize();
    LoadConfig();
    SteamHookInstall(dedicated);
}
