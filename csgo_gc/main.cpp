#include "stdafx.h"
#include "platform.h"
#include "steam_hook.h"

void InstallGC(bool dedicated)
{
    Platform::Initialize();
    SteamHookInstall(dedicated);
}
