#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <dlfcn.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

#if defined(DEDICATED)
#define LAUNCHER_LIB "dedicated"
#define SYMBOL_NAME "DedicatedMain"
#else
#define LAUNCHER_LIB "launcher"
#define SYMBOL_NAME "LauncherMain"
#endif

typedef int(*LauncherMain_t)(int argc, char **argv);
typedef void (*InstallGC_t)(bool dedicated);

static void ErrorMessageBox(const char *format, ...)
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
}

static void *LoadModuleAndFindSymbol(const char *modulePath, const char *symbol)
{
    void *module = dlopen(modulePath, RTLD_NOW);
    if (!module)
    {
        ErrorMessageBox("%s", dlerror());
        return nullptr;
    }

    void *function = dlsym(module, symbol);
    if (!function)
    {
        ErrorMessageBox("%s", dlerror());
        return nullptr;
    }

    return function;
}

int main(int argc, char **argv)
{
    const char *modulePath = "bin/" GC_LIB_DIR "/" LAUNCHER_LIB GC_LIB_SUFFIX GC_LIB_EXTENSION;
    LauncherMain_t LauncherMain = (LauncherMain_t)LoadModuleAndFindSymbol(modulePath, SYMBOL_NAME);
    if (!LauncherMain)
    {
        // LoadModuleAndFindSymbol told us why
        return 1;
    }

    modulePath = "csgo_gc/" GC_LIB_DIR "/" "csgo_gc" GC_LIB_EXTENSION;
    InstallGC_t InstallGC = (InstallGC_t)LoadModuleAndFindSymbol(modulePath, "InstallGC");
    if (!InstallGC)
    {
        // LoadModuleAndFindSymbol told us why
        return 1;
    }

#if defined(DEDICATED)
    InstallGC(true);
#else
    InstallGC(false);
#endif

    return LauncherMain(argc, argv);
}
