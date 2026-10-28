#include "stdafx.h"
#include "steamutilsproxy.h"

// stupid hack
constexpr SteamAPICall_t CheckSignatureCall = 0x6666666666666666;

PROXY_FUNC(bool, SteamUtilsProxy::IsAPICallCompleted, SteamAPICall_t hSteamAPICall, bool *pbFailed)
{
    if (hSteamAPICall == CheckSignatureCall)
    {
        if (pbFailed)
        {
            *pbFailed = false;
        }

        return true;
    }

    return original(hSteamAPICall, pbFailed);
}

// yeah we won't get here
//PROXY_FUNC(ESteamAPICallFailure, GetAPICallFailureReason, SteamAPICall_t hSteamAPICall)
//{
//    if (hSteamAPICall == CheckSignatureCall)
//    {
//        // not properly handled, shouldn't get here
//        assert(false);
//        return k_ESteamAPICallFailureNone;
//    }
//
//    return original(hSteamAPICall);
//}

PROXY_FUNC(bool, SteamUtilsProxy::GetAPICallResult, SteamAPICall_t hSteamAPICall, void *pCallback, int cubCallback, int iCallbackExpected, bool *pbFailed)
{
    if (hSteamAPICall == CheckSignatureCall
        && cubCallback == sizeof(CheckFileSignature_t)
        && iCallbackExpected == CheckFileSignature_t::k_iCallback)
    {
        if (pbFailed)
        {
            *pbFailed = false;
        }

        CheckFileSignature_t result{};
        result.m_eCheckFileSignature = k_ECheckFileSignatureNoSignaturesFoundForThisApp;
        memcpy(pCallback, &result, sizeof(result));
        return true;
    }

    return original(hSteamAPICall, pCallback, cubCallback, iCallbackExpected, pbFailed);
}

PROXY_FUNC(SteamAPICall_t, SteamUtilsProxy::CheckFileSignature, const char *)
{
    // spoof this
    return CheckSignatureCall;
}
