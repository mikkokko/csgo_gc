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

void SetEnvVar(const char *name, const char *value)
{
    SetEnvironmentVariableA(name, value);
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

// FIXME: generalize and use for graffiti public key as well
static bool GetCodeSection(HMODULE module, uint8_t **pstart, uint8_t **pend)
{
    uint8_t *base = reinterpret_cast<uint8_t *>(module);

    PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
    PIMAGE_NT_HEADERS ntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dosHeader->e_lfanew);

    PIMAGE_SECTION_HEADER sectionHeader = IMAGE_FIRST_SECTION(ntHeaders);

    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++)
    {
        if (sectionHeader[i].Characteristics & IMAGE_SCN_CNT_CODE)
        {
            DWORD sectionStart = sectionHeader[i].VirtualAddress;
            DWORD sectionSize = sectionHeader[i].Misc.VirtualSize;
            *pstart = base + sectionStart;
            *pend = *pstart + sectionSize;
            return true;
        }
    }

    return false;
}

static uint8_t *FindUint32FromCode(uint8_t *start, uint8_t *end, uint32_t value)
{
    void *result = Q_memmem(start, end - start, &value, sizeof(value));
    return static_cast<uint8_t *>(result);
}

// the server browser filters out servers with appid < 200 or > 900 unless it's garry's mod,
// so replace gmod appid (4000) with the requested one
bool PatchServerBrowserAppId(uint32_t appId)
{
    HMODULE module = GetModuleHandleA("serverbrowser.dll");
    if (!module)
    {
        // shouldn't happen
        return false;
    }

    uint8_t *codeStart, *codeEnd;
    if (!GetCodeSection(module, &codeStart, &codeEnd))
    {
        // shouldn't happen
        return false;
    }

    // FIXME: what would be an actually suitable delta? 64 is an overkill
    constexpr size_t MaxDelta = 64;
    uint8_t *searchStart = codeStart;

    while (searchStart < codeEnd)
    {
        // look for gmod first
        uint8_t *ptr4000 = FindUint32FromCode(searchStart, codeEnd, 4000);
        if (!ptr4000)
        {
            break;
        }

        // look up min and max addresses for 200 and 900
        // this is effectively always an overkill, but we want to make sure
        uint8_t *rangeStart = (ptr4000 >= codeStart + MaxDelta) ? (ptr4000 - MaxDelta) : codeStart;
        uint8_t *rangeEnd = (ptr4000 + MaxDelta <= codeEnd) ? (ptr4000 + MaxDelta) : codeEnd;

        // might compile to 199 or 901, so check all 4 and patch if 2 are found (bruh)
        bool foundLow = FindUint32FromCode(rangeStart, rangeEnd, 200);
        if (!foundLow)
        {
            foundLow = FindUint32FromCode(rangeStart, rangeEnd, 199);
        }

        bool foundHigh = FindUint32FromCode(rangeStart, rangeEnd, 900);
        if (!foundHigh)
        {
            foundHigh = FindUint32FromCode(rangeStart, rangeEnd, 901);
        }

        if (foundLow && foundHigh)
        {
            DWORD oldProtect;

            // if we find both, this should be the address we need to patch --> replace gmod appid with the target one
            if (VirtualProtect(ptr4000, sizeof(uint32_t), PAGE_EXECUTE_READWRITE, &oldProtect))
            {
                *reinterpret_cast<uint32_t *>(ptr4000) = appId;
                VirtualProtect(ptr4000, sizeof(uint32_t), oldProtect, &oldProtect);

                // might want to do this since we're patching so late
                FlushInstructionCache(GetCurrentProcess(), ptr4000, sizeof(uint32_t));
            }
            else
            {
                return false;
            }

            return true;
        }

        searchStart = ptr4000 + 1;
    }

    return false;
}

} // namespace Platform
