#include "stdafx.h"
#include "platform.h"
#include <windows.h>

namespace Platform
{

using ConColorMsg_t = void (*)(const uint8_t *, const char *, ...);
static ConColorMsg_t s_ConColorMsg;

void Initialize()
{
    // remove the old log file
    DeleteFileA("gc_log.txt");

    HMODULE tier0 = GetModuleHandleW(L"tier0.dll");
    if (tier0)
    {
        s_ConColorMsg = (ConColorMsg_t)GetProcAddress(tier0, "?ConColorMsg@@YAXABVColor@@PBDZZ");
    }
}

void Print(const char *format, ...)
{
    va_list ap;
    char buffer[4096];

    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    if (s_ConColorMsg)
    {
        uint8_t color[4] = { 0, 255, 128, 255 };
        s_ConColorMsg(color, "[GC] %s", buffer);
    }

    FILE *f = fopen("gc_log.txt", "a");
    fprintf(f, "%s", buffer);
    fclose(f);
}

void Error(const char *format, ...)
{
    va_list ap;
    char buffer[4096];

    va_start(ap, format);
    vsnprintf(buffer, sizeof(buffer), format, ap);
    va_end(ap);

    MessageBoxA(nullptr, buffer, "csgo_gc", MB_OK | MB_ICONERROR);
    ExitProcess(1);
}

bool SteamClientPath(void *buffer, size_t bufferSize)
{
    HMODULE steamclient = GetModuleHandleW(L"steamclient.dll");
    DWORD result = GetModuleFileNameW(steamclient, reinterpret_cast<wchar_t *>(buffer), bufferSize / sizeof(wchar_t));
    return (result > 0 && result < bufferSize);
}

void *SteamClientFactory(const void *pathBuffer)
{
    HMODULE steamclient = LoadLibraryExW(
        reinterpret_cast<const wchar_t *>(pathBuffer),
        nullptr,
        LOAD_WITH_ALTERED_SEARCH_PATH);

    if (!steamclient)
    {
        return nullptr;
    }

    return GetProcAddress(steamclient, "CreateInterface");
}

void EnsureEnvVarSet(const char *name, const char *value)
{
    if (!GetEnvironmentVariableA(name, nullptr, 0))
    {
        SetEnvironmentVariableA(name, value);
    }
}

static void *Q_memmem(const void *_haystack, size_t haystack_len, const void *_needle, size_t needle_len)
{
    uint8_t *haystack = (uint8_t *)_haystack;
    uint8_t *needle = (uint8_t *)_needle;

    uint8_t *ptr = haystack;
    uint8_t *end = haystack + haystack_len;

    while ((size_t)(end - ptr) >= needle_len)
    {
        ptr = (uint8_t *)memchr(ptr, *needle, end - ptr);
        if (!ptr)
        {
            return NULL;
        }

        if (!memcmp(ptr, needle, needle_len))
        {
            return ptr;
        }

        ptr++;
    }

    return NULL;
}

bool PatchGraffitiPublicKey(std::string_view moduleName, const void *original, const void *replacement, size_t size)
{
    std::string actualModuleName;
    actualModuleName.assign(moduleName);
    actualModuleName.append(".dll");

    HMODULE module = GetModuleHandleA(actualModuleName.c_str());
    if (!module)
    {
        return false;
    }

    LPBYTE moduleBase = reinterpret_cast<LPBYTE>(module);
    PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(moduleBase);
    PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(moduleBase + dosHeader->e_lfanew);
    DWORD moduleSize = ntHeaders->OptionalHeader.SizeOfImage;

    void *address = Q_memmem(moduleBase, moduleSize, original, size);
    if (!address)
    {
        return false;
    }

    DWORD oldProtect;
    if (VirtualProtect(address, size, PAGE_READWRITE, &oldProtect))
    {
        memcpy(address, replacement, size);
        VirtualProtect(address, size, oldProtect, &oldProtect);
        return true;
    }

    return false;
}

} // namespace Platform
