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

    auto callback = [](dl_phdr_info *info, size_t, void *data)
    {
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

void SetEnvVar(const char *name, const char *value)
{
    setenv(name, value, 1);
}

static void CopyToReadOnly(void *dest, const void *src, size_t size, int oldFlags)
{
    size_t pageSize = sysconf(_SC_PAGESIZE);
    void *alignedAddress = (void *)((size_t)dest & ~(pageSize - 1));

    size_t alignedSize = ((size_t)dest + size) - (size_t)alignedAddress;

    assert(alignedAddress <= dest);
    assert(alignedSize >= size);

    int result = mprotect(alignedAddress, alignedSize, oldFlags | PROT_WRITE);
    assert(result == 0);

    memcpy(dest, src, size);

    result = mprotect(alignedAddress, alignedSize, oldFlags);
    assert(result == 0);
}

bool PatchGraffitiPublicKey(std::string_view moduleName, const void *original, const void *replacement, size_t size)
{
    std::string actualModuleName;

#if defined(__APPLE__)
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

    CopyToReadOnly(address, replacement, size, PROT_READ);
    return true;
}

// FIXME: generalize and use for graffiti public key as well
#if defined(__APPLE__)
static bool GetModuleCodeSection(mach_header_64 *header, intptr_t slide, uint8_t **pstart, uint8_t **pend)
{
    uint8_t *cmd = (uint8_t *)(header + 1);

    for (uint32_t i = 0; i < header->ncmds; i++)
    {
        load_command *lc = (load_command *)cmd;
        if (lc->cmd == LC_SEGMENT_64)
        {
            segment_command_64 *seg = (segment_command_64 *)lc;
            if (!strcmp(seg->segname, "__TEXT"))
            {
                section_64 *sect = (section_64 *)(seg + 1);
                for (uint32_t j = 0; j < seg->nsects; j++)
                {
                    if (!strcmp(sect[j].sectname, "__text"))
                    {
                        *pstart = reinterpret_cast<uint8_t *>(sect[j].addr + slide);
                        *pend = *pstart + sect[j].size;
                        return true;
                    }
                }
            }
        }

        cmd += lc->cmdsize;
    }

    return false;
}

static bool GetCodeSection(const char *name, uint8_t **pstart, uint8_t **pend)
{
    uint32_t imageCount = _dyld_image_count();

    for (uint32_t i = 0; i < imageCount; i++)
    {
        const char *fullName = _dyld_get_image_name(i);
        if (fullName && strstr(fullName, name))
        {
            // assume 64 bit, don't even check
            const mach_header_64 *header = (const mach_header_64 *)_dyld_get_image_header(i);
            intptr_t slide = _dyld_get_image_vmaddr_slide(i);
            return GetModuleCodeSection(header, slide, pstart, pend);
        }
    }

    return false;
}
#else
static bool GetCodeSection(const char *name, uint8_t **pstart, uint8_t **pend)
{
    struct SectionInfo
    {
        const char *searchName;
        uint8_t *start, *end;
    };

    SectionInfo info{};
    info.searchName = name;

    auto callback = [](dl_phdr_info *info, size_t, void *data)
    {
        SectionInfo *sectionInfo = static_cast<SectionInfo *>(data);

        if (strstr(info->dlpi_name, sectionInfo->searchName))
        {
            for (int i = 0; i < info->dlpi_phnum; i++)
            {
                if (info->dlpi_phdr[i].p_type == PT_LOAD && (info->dlpi_phdr[i].p_flags & PF_X))
                {
                    sectionInfo->start = reinterpret_cast<uint8_t *>(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
                    sectionInfo->end = sectionInfo->start + info->dlpi_phdr[i].p_memsz;
                    return 1;
                }
            }
        }

        return 0;
    };

    if (dl_iterate_phdr(callback, &info))
    {
        *pstart = info.start;
        *pend = info.end;
        return true;
    }

    return false;
}
#endif

static uint8_t *FindUint32FromCode(uint8_t *start, uint8_t *end, uint32_t value)
{
    void *result = memmem(start, end - start, &value, sizeof(value));
    return static_cast<uint8_t *>(result);
}

// the server browser filters out servers with appid < 200 or > 900 unless it's garry's mod,
// so replace gmod appid (4000) with the requested one
bool PatchServerBrowserAppId(uint32_t appId)
{
    uint8_t *codeStart, *codeEnd;

#if defined(__APPLE__)
    bool success = GetCodeSection("serverbrowser.dylib", &codeStart, &codeEnd);
#else
    bool success = GetCodeSection("serverbrowser_client.so", &codeStart, &codeEnd);
    if (!success)
    {
        success = GetCodeSection("serverbrowser.so", &codeStart, &codeEnd);
    }
#endif

    if (!success)
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
            CopyToReadOnly(ptr4000, &appId, sizeof(uint32_t), PROT_READ | PROT_EXEC);

            // might want to do this since we're patching so late
            __builtin___clear_cache((char *)ptr4000, (char *)ptr4000 + sizeof(uint32_t));

            return true;
        }

        searchStart = ptr4000 + 1;
    }

    return false;
}

} // namespace Platform
