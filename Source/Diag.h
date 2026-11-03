//  Diag.h — mouchard de plantage.
//
//  But : quand le plugin tombe, écrire NOUS-MÊMES, sur le disque du musicien,
//  l'étape exacte où ça s'est produit ("paint:gauge", "timer", ...) et l'adresse
//  fautive ramenée en offset de module. Ça évite de dépendre du fichier .map du
//  binaire, qui n'est pas livré avec le plugin.
//
//  Coût sur le fil audio : un simple store atomique. Aucune écriture disque
//  sauf en cas de plantage.
#pragma once

namespace ht
{
namespace diag
{
    /** Installe le mouchard (idempotent). À appeler au plus tôt. */
    void install() noexcept;

    /** Nomme l'étape courante : c'est ce nom qui sera écrit si ça plante. */
    void stage (const char* name) noexcept;

    /** Écrit une ligne horodatée dans %TEMP%\\hardtune-diag.txt. */
    void log (const char* what) noexcept;

    /** Écrit une ligne seulement la première fois (évite d'inonder le disque). */
    void logOnce (const char* what) noexcept;

    /** Chemin du journal (lecture seule, pour l'afficher dans l'interface). */
    const char* logPath() noexcept;
}
} // namespace ht
