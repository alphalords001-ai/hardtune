//  Diag.h — mouchard de plantage.
//
//  But : quand le plugin tombe, écrire NOUS-MÊMES, sur le disque du musicien,
//  l'étape exacte où ça s'est produit ("paint:gauge", "timer", ...) et l'adresse
//  fautive ramenée en offset de module. Ça évite de dépendre du fichier .map du
//  binaire, qui n'est pas livré avec le plugin.
//
//  Coût sur le fil audio : un simple store atomique, aucune écriture disque.
//  Le fil d'interface écrit au plus ~6 lignes par seconde.
#pragma once

namespace ht
{
namespace diag
{
    /** Installe le mouchard (idempotent). À appeler au plus tôt. */
    void install() noexcept;

    /** Déclare le fil courant comme fil d'interface : seul ce fil écrit le
        journal (le fil audio n'a jamais le droit de toucher au disque). */
    void markUiThread() noexcept;

    /** Nomme l'étape courante et l'écrit (au plus ~6 lignes par seconde).
        Après une fermeture brutale, la dernière ligne du journal indique
        exactement la fonction où le plugin se trouvait. */
    void stage (const char* name) noexcept;

    /** Écrit une ligne horodatée dans %TEMP%\\hardtune-diag.txt. */
    void log (const char* what) noexcept;

    /** Écrit une ligne seulement la première fois (évite d'inonder le disque). */
    void logOnce (const char* what) noexcept;

    /** Chemin du journal (lecture seule, pour l'afficher dans l'interface). */
    const char* logPath() noexcept;

    /** Vrai si un fichier marqueur existe dans le dossier temporaire (%TEMP%). */
    bool flagExists (const char* name) noexcept;
}
} // namespace ht
