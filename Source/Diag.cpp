//  Diag.cpp — voir Diag.h.
#include "Diag.h"

#include <atomic>
#include <cstdio>

#if defined (_WIN32)
 #include <windows.h>
#endif

namespace ht
{
namespace diag
{

namespace
{
    std::atomic<const char*> g_stage { "demarrage" };
    std::atomic<bool>        g_installed { false };
    char                     g_path[512] = {};

    void buildPath()
    {
        if (g_path[0] != 0)
            return;

#if defined (_WIN32)
        char tmp[MAX_PATH] = {};
        const DWORD n = GetTempPathA (MAX_PATH, tmp);

        if (n > 0 && n < MAX_PATH)
            std::snprintf (g_path, sizeof (g_path), "%shardtune-diag.txt", tmp);
        else
            std::snprintf (g_path, sizeof (g_path), "hardtune-diag.txt");
#else
        std::snprintf (g_path, sizeof (g_path), "/tmp/hardtune-diag.txt");
#endif
    }

    void append (const char* text) noexcept
    {
        buildPath();

        if (FILE* f = std::fopen (g_path, "a"))
        {
            std::fprintf (f, "%s\n", text != nullptr ? text : "");
            std::fclose (f);
        }
    }

#if defined (_WIN32)
    LONG CALLBACK vectoredHandler (EXCEPTION_POINTERS* info)
    {
        if (info == nullptr || info->ExceptionRecord == nullptr)
            return EXCEPTION_CONTINUE_SEARCH;

        const DWORD code = info->ExceptionRecord->ExceptionCode;

        // 0xC0000005 = access violation (pointeur nul / memoire invalide)
        // 0xC0000409 = pile corrompue / depassement de tampon detecte
        if (code != 0xC0000005u && code != 0xC0000409u)
            return EXCEPTION_CONTINUE_SEARCH;

        const void* addr = info->ExceptionRecord->ExceptionAddress;

        char moduleName[MAX_PATH] = "?";
        unsigned long long offset = 0;

        HMODULE mod = nullptr;
        if (GetModuleHandleExA (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                    | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                (LPCSTR) addr, &mod)
            && mod != nullptr)
        {
            GetModuleFileNameA (mod, moduleName, MAX_PATH);
            offset = (unsigned long long) ((const char*) addr - (const char*) mod);
        }

        const unsigned long long bad = (code == 0xC0000005u
                                            && info->ExceptionRecord->NumberParameters > 1)
                                           ? (unsigned long long) info->ExceptionRecord->ExceptionInformation[1]
                                           : 0ull;

        const char* stageName = g_stage.load();

        char line[1400];
        std::snprintf (line, sizeof (line),
                       "PLANTAGE code=0x%08lX module=%s +0x%llX adresse_fautive=0x%llX etape=%s",
                       (unsigned long) code, moduleName, offset, bad,
                       stageName != nullptr ? stageName : "?");

        append (line);
        return EXCEPTION_CONTINUE_SEARCH;   // on laisse le plantage suivre son cours
    }
#endif
}

const char* logPath() noexcept
{
    buildPath();
    return g_path;
}

void install() noexcept
{
    bool expected = false;
    if (! g_installed.compare_exchange_strong (expected, true))
        return;

    append ("--- HardTune : demarrage du mouchard ---");

#if defined (_WIN32)
    AddVectoredExceptionHandler (1, vectoredHandler);
#endif
}

void stage (const char* name) noexcept
{
    g_stage.store (name, std::memory_order_relaxed);
}

void log (const char* what) noexcept
{
    append (what);
}

void logOnce (const char* what) noexcept
{
    static std::atomic<bool> done { false };
    bool expected = false;

    if (done.compare_exchange_strong (expected, true))
        append (what);
}

} // namespace diag
} // namespace ht
