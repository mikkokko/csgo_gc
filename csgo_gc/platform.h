#pragma once

namespace Platform
{

// called right after we load into the process
void Initialize();


// print a debugging message to the in game console or something
void Print(const char *format, ...);

// fatal error, show a message box if possible and exit the program
[[noreturn]] void Error(const char *format, ...);

// get steamclient's path without incrementing its refcount
// buffer is only accessed by the platform code, on
// windows we stick utf16 to it and utf8 on other platforms
bool SteamClientPath(void *buffer, size_t bufferSize);

// load steamclient from the provided path, increment its refcount
// and get a pointer to the factory function (exported symbol CreateInterface)
void *SteamClientFactory(const void *pathBuffer);

// if the env var is not set, set it to the specified value
void EnsureEnvVarSet(const char *name, const char *value);

// patch the graffiti public key in the specified module to get sprays working
// the module name is given in a platform-agnostic format
// (e.g. on linux pass server as moduleName, and it'll operate on server_client.so)
bool PatchGraffitiPublicKey(std::string_view moduleName, const void *original, const void *replacement, size_t size);

} // namespace Platform