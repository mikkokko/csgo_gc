#pragma once

#include <proxy/steamproxy.h>
#include <steam/isteamutils.h>

// hook so we can spoof the dll signature checks and get rid of the annoying as fuck insecure message box
class SteamUtilsProxy final
{
public:
    PROXY_FUNC(bool, IsAPICallCompleted, SteamAPICall_t hSteamAPICall, bool *pbFailed);
    PROXY_FUNC(bool, GetAPICallResult, SteamAPICall_t hSteamAPICall, void *pCallback, int cubCallback, int iCallbackExpected, bool *pbFailed);
    PROXY_FUNC(SteamAPICall_t, CheckFileSignature, const char *);
};
