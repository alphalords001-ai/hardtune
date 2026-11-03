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
    std::atomic<const char*>  g_stage    { "demarrage" };
    std::atomic<bool>         g_installed { false };
    std::atomic<unsigned long> g_uiThread { 0 };     // identifiant du fil qui a cree l'editeur
    std::atomic<unsigned long> g_lastWrite { 0 };    // anti-inondation (ms)
    char                      g_path[512] = {};

    unsigned long currentThreadId() noexcept
    {
#if defined (_WIN32)
        return (unsigned long) GetCurrentThreadId();
#else
        return 0;
#endif
    }

    unsigned long nowMs() noexcept
    {
#if defined (_WIN32)
        return (unsigned long) GetTickCount64();
#else
        return 0;
#endif
    }

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

    //  Ecriture disque : UNIQUEMENT depuis le fil d'interface et jamais pendant
    //  le chargement du plugin. Aucun autre fil (audio, hote, loader) n'ecrit
    //  ici : c'est ce qui a rendu la v0.5.4 instable.
    void append (const char* text) noexcept
    {
        buildPath();

        if (FILE* f = std::fopen (g_path, "a"))
        {
            std::fprintf (f, "%s\n", text != nullptr ? text : "");
            std::fclose (f);
        }
    }
}

void install() noexcept
{
    //  Volontairement inoffensif : pas de handler d'exceptions global, pas
    //  d'ecriture disque. Ajouter un handler vecteurise ici a chaque access
    //  violation du processus entier fait tomber le DAW au chargement (v0.5.4).
    g_installed.store (true);
}

void markUiThread() noexcept
{
    g_uiThread.store (currentThreadId());
}

void stage (const char* name) noexcept
{
    g_stage.store (name, std::memory_order_relaxed);

    //  On n'ecrit que depuis le fil de l'interface : le fil audio n'a
    //  jamais le droit de toucher au disque.
    const auto ui = g_uiThread.load (std::memory_order_relaxed);

    if (ui == 0 || currentThreadId() != ui)
        return;

    //  Au plus ~6 lignes par seconde : le journal reste lisible et le cout
    //  reste negligible a 30 images par seconde.
    const auto now = nowMs();

    if (now - g_lastWrite.load (std::memory_order_relaxed) < 150)
        return;

    g_lastWrite.store (now, std::memory_order_relaxed);
    append (name);
}

void log (const char* what) noexcept
{
    append (what);
}

bool flagExists (const char* name) noexcept
{
#if defined (_WIN32)
    char tmp[MAX_PATH] = {};
    const DWORD n = GetTempPathA (MAX_PATH, tmp);

    if (n == 0 || n >= MAX_PATH || name == nullptr)
        return false;

    char full[MAX_PATH * 2] = {};
    std::snprintf (full, sizeof (full), "%s%s", tmp, name);
    return GetFileAttributesA (full) != INVALID_FILE_ATTRIBUTES;
#else
    (void) name;
    return false;
#endif
}

void logOnce (const char* what) noexcept
{
    static std::atomic<bool> done { false };
    bool expected = false;

    if (done.compare_exchange_strong (expected, true))
        append (what);
}

const char* logPath() noexcept
{
    buildPath();
    return g_path;
}

} // namespace diag
} // namespace ht
