#include <windows.h>
#include <wchar.h>
#include <array>
#include <string>

#if !defined(DEDICATED)

#define DLL_EXPORT extern "C" __declspec(dllexport)

DLL_EXPORT DWORD NvOptimusEnablement = 1;
DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1;

DLL_EXPORT bool BSecureAllowed(unsigned char *, int, int)
{
    return true;
}

DLL_EXPORT int CountFilesCompletedTrustCheck()
{
    return 0;
}

DLL_EXPORT int CountFilesNeedTrustCheck()
{
    return 0;
}

DLL_EXPORT int GetTotalFilesLoaded()
{
    return 0;
}

DLL_EXPORT int RuntimeCheck(int, int)
{
    return 0;
}

#endif

#if defined(DEDICATED)
#define LAUNCHER_LIB "dedicated"
#define SYMBOL_NAME "DedicatedMain"
typedef int(*LauncherMain_t)(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd);
#else
#define LAUNCHER_LIB "launcher"
#define SYMBOL_NAME "LauncherMain"
typedef int(*LauncherMain_t)(bool bSecure, HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd);
#endif

typedef void (*InstallGC_t)(bool dedicated);

static void ErrorMessageBox(const wchar_t *format, ...)
{
    va_list ap;
    wchar_t buffer[4096];

    va_start(ap, format);
    _vsnwprintf_s(buffer, std::size(buffer), format, ap);
    va_end(ap);

    MessageBoxW(nullptr, buffer, L"csgo_gc", MB_OK | MB_ICONERROR);
}

static const wchar_t *LastErrorString()
{
    static wchar_t buffer[4096];

    buffer[0] = '\0';

    int error = GetLastError();

    int result = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS
        | FORMAT_MESSAGE_MAX_WIDTH_MASK,
        nullptr,
        error,
        0,
        buffer,
        std::size(buffer),
        nullptr);

    if (!result)
    {
        _snwprintf_s(buffer, std::size(buffer), L"Unknown error (%d)", error);
    }

    return buffer;
}

static void *LoadModuleAndFindSymbol(const wchar_t *abosoluteModulePath, const char *symbol)
{
    HMODULE module = LoadLibraryExW(abosoluteModulePath, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module)
    {
        ErrorMessageBox(L"Could not load '%s':\n%s", abosoluteModulePath, LastErrorString());
        return nullptr;
    }

    void *function = GetProcAddress(module, symbol);
    if (!function)
    {
        ErrorMessageBox(L"Could not find '%S' from '%s':\n%s", symbol, abosoluteModulePath, LastErrorString());
        return nullptr;
    }

    return function;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    wchar_t baseDir[MAX_PATH];
    wchar_t modulePath[MAX_PATH];

    DWORD baseDirLength = GetModuleFileNameW(nullptr, baseDir, std::size(baseDir));
    if (!baseDirLength || baseDirLength == std::size(baseDir))
    {
        ErrorMessageBox(L"GetModuleFileName failed:\n%ls", LastErrorString());
        return 1;
    }

    // rip off exe from the path
    wchar_t *slash = wcsrchr(baseDir, '\\');
    if (!slash)
    {
        slash = baseDir; // what the fuck
    }

    *slash = '\0';

    // add bin dir to PATH
    {
        // allocate this on the heap
        std::wstring replacePath;
        replacePath.reserve(2048);

        replacePath.append(baseDir);
        replacePath.append(L"\\bin\\" GC_LIB_DIR "\\;");

        const wchar_t *currentPath = _wgetenv(L"PATH");
        if (currentPath)
        {
            replacePath.append(currentPath);
        }

        _wputenv_s(L"PATH", replacePath.c_str());
    }

    _snwprintf_s(modulePath, std::size(modulePath), L"%ls\\bin\\" GC_LIB_DIR "\\" LAUNCHER_LIB GC_LIB_SUFFIX GC_LIB_EXTENSION, baseDir);
    LauncherMain_t LauncherMain = (LauncherMain_t)LoadModuleAndFindSymbol(modulePath, SYMBOL_NAME);
    if (!LauncherMain)
    {
        // LoadModuleAndFindSymbol told us why
        return 1;
    }

    _snwprintf_s(modulePath, std::size(modulePath), L"%ls\\csgo_gc\\" GC_LIB_DIR "\\" "csgo_gc" GC_LIB_EXTENSION, baseDir);
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

#if defined(DEDICATED)
    return LauncherMain(hInstance, hPrevInstance, lpCmdLine, nShowCmd);
#else
    return LauncherMain(true, hInstance, hPrevInstance, lpCmdLine, nShowCmd);
#endif
}
