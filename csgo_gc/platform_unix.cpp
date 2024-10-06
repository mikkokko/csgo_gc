#include "stdafx.h"
#include "platform.h"
#include <dlfcn.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/mman.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <CoreFoundation/CoreFoundation.h>
#else
#include <link.h>
#endif

namespace Platform
{

using ConColorMsg_t = void (*)(const uint8_t *, const char *, ...);
static ConColorMsg_t s_ConColorMsg;

void Initialize()
{
    // remove the old log file
    unlink("gc_log.txt");

#if defined(__APPLE__)
    void *tier0 = dlopen("libtier0.dylib", RTLD_LAZY);
#else
    // try client first
    void *tier0 = dlopen("libtier0_client.so", RTLD_LAZY);
    if (!tier0)
    {
        // try dedicated
        tier0 = dlopen("libtier0.so", RTLD_LAZY);
    }
#endif

    if (tier0)
    {
        s_ConColorMsg = (ConColorMsg_t)dlsym(tier0, "_Z11ConColorMsgRK5ColorPKcz");
        dlclose(tier0);
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
    char message[4096];

    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    va_end(ap);

    fprintf(stderr, "csgo_gc: %s\n", message);

#if defined(__APPLE__)
    CFStringRef messageString = CFStringCreateWithCString(kCFAllocatorDefault, message, kCFStringEncodingUTF8);

    CFOptionFlags responseFlags;

    CFUserNotificationDisplayAlert(0, // timeout
        kCFUserNotificationStopAlertLevel, // docs: "The notification is very serious."
        nullptr, // iconURL
        nullptr, // soundURL
        nullptr, // localizationURL
        CFSTR("csgo_gc"), // alertHeader
        messageString, // alertMessage
        nullptr, // defaultButtonTitle
        nullptr, // alternateButtonTitle
        nullptr, // otherButtonTitle
        &responseFlags); // not used but idk if it can be null

    CFRelease(messageString);
#else
    // display a message box if we can
    // i don't want to link to sdl2 directly nor do i want it as a build dependency
    void *libSDL2 = dlopen("libSDL2.so", RTLD_LAZY);
    if (libSDL2)
    {
        using SDL_ShowSimpleMessageBox_t = int (*)(unsigned, const char *, const char *, void *);
        SDL_ShowSimpleMessageBox_t fuction = reinterpret_cast<SDL_ShowSimpleMessageBox_t>(dlsym(libSDL2, "SDL_ShowSimpleMessageBox"));
        if (fuction)
        {
            fuction(0, "csgo_gc", message, nullptr);
        }

        dlclose(libSDL2);
    }
#endif

    exit(1);
}

struct ModuleInfo
{
    std::string fullPath;
    void *baseAddress;
    size_t size;
};

#if defined(__APPLE__)
static size_t ImageSize64(mach_header_64 *header)
{
    size_t size = sizeof(*header) + header->sizeofcmds;

    load_command *loadCommand = (load_command *)(header + 1);

    for (uint32_t i = 0; i < header->ncmds; i++)
    {
        if (loadCommand->cmd == LC_SEGMENT_64)
        {
            size += ((segment_command_64 *)loadCommand)->vmsize;
        }

        loadCommand = (load_command *)((uint8_t *)loadCommand + loadCommand->cmdsize);
    }

    return size;
}

static bool GetModuleInfo(std::string_view moduleName, ModuleInfo &info)
{
    std::string search;
    search.assign("/"); // so that client won't return results for steamclient for example
    search.append(moduleName);

    uint32_t imageCount = _dyld_image_count();

    for (uint32_t i = 0; i < imageCount; i++)
    {
        const char *fullName = _dyld_get_image_name(i);
        if (fullName && strstr(fullName, search.c_str()))
        {
            // assume 64 bit, don't even check
            mach_header_64 *header = (mach_header_64 *)_dyld_get_image_header(i);
            if (!header)
            {
                assert(false);
                return false;
            }

            info.fullPath = fullName;
            info.baseAddress = header;
            info.size = ImageSize64(header);
            return true;
        }
    }

    return false;
}
#else
static bool GetModuleInfo(std::string_view moduleName, ModuleInfo &info)
{
    // abuse the fullPath field for searching
    info.fullPath.assign("/"); // so that client won't return results for steamclient for example
    info.fullPath.append(moduleName);

    auto callback = [](dl_phdr_info *info, size_t size, void *data) {
        ModuleInfo *moduleInfo = static_cast<ModuleInfo *>(data);

        if (strstr(info->dlpi_name, moduleInfo->fullPath.c_str()))
        {
            moduleInfo->fullPath.assign(info->dlpi_name);
            moduleInfo->baseAddress = reinterpret_cast<void *>(info->dlpi_addr + info->dlpi_phdr[0].p_vaddr);
            moduleInfo->size = info->dlpi_phdr[0].p_memsz;
            return 1;
        }

        return 0;
        };

    return dl_iterate_phdr(callback, &info) ? true : false;
}
#endif

bool SteamClientPath(void *buffer, size_t bufferSize)
{
    ModuleInfo moduleInfo;
#if defined(__APPLE__)
    if (!GetModuleInfo("steamclient.dylib", moduleInfo))
#else
    if (!GetModuleInfo("steamclient.so", moduleInfo))
#endif
    {
        return false;
    }

    if (moduleInfo.fullPath.size() >= bufferSize)
    {
        return false;
    }

    memcpy(buffer, moduleInfo.fullPath.data(), moduleInfo.fullPath.size() + 1);
    return true;
}

void *SteamClientFactory(const void *pathBuffer)
{
    void *steamclient = dlopen(reinterpret_cast<const char *>(pathBuffer), RTLD_NOW);
    if (!steamclient)
    {
        return nullptr;
    }

    return dlsym(steamclient, "CreateInterface");
}

void EnsureEnvVarSet(const char *name, const char *value)
{
    setenv(name, value, 0);
}

static void CopyToReadOnly(void *dest, const void *src, size_t size)
{
    size_t pageSize = sysconf(_SC_PAGESIZE);
    void *alignedAddress = (void *)((size_t)dest & ~(pageSize - 1));

    size_t alignedSize = ((size_t)dest + size) - (size_t)alignedAddress;

    assert(alignedAddress <= dest);
    assert(alignedSize >= size);

    int result = mprotect(alignedAddress, alignedSize, PROT_READ | PROT_WRITE);
    assert(result == 0);

    memcpy(dest, src, size);

    result = mprotect(alignedAddress, alignedSize, PROT_READ);
    assert(result == 0);
}

bool PatchGraffitiPublicKey(std::string_view moduleName, const void *original, const void *replacement, size_t size)
{
    std::string actualModuleName;

#if defined(__APPLE)
    actualModuleName.assign(moduleName);
    actualModuleName.append(".dylib");

    ModuleInfo moduleInfo;
    if (!GetModuleInfo(actualModuleName, moduleInfo))
    {
        return false;
    }
#else
    // try client library first
    actualModuleName.assign(moduleName);
    actualModuleName.append("_client.so");

    ModuleInfo moduleInfo;
    if (!GetModuleInfo(actualModuleName, moduleInfo))
    {
        // try server library
        actualModuleName.assign(moduleName);
        actualModuleName.append(".so");

        if (!GetModuleInfo(actualModuleName, moduleInfo))
        {
            return false;
        }
    }
#endif

    void *address = memmem(moduleInfo.baseAddress, moduleInfo.size, original, size);
    if (!address)
    {
        return false;
    }

    CopyToReadOnly(address, replacement, size);
    return true;
}

} // namespace Platform
