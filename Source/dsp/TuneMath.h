//  TuneMath.h — petits utilitaires de calcul de hauteur.
//  Aucune dépendance JUCE : ce fichier est compilable en ligne de commande.
#pragma once

#include <cmath>
#include <algorithm>

namespace tune
{
    constexpr double kPi = 3.14159265358979323846;

    /** Ratio de fréquence correspondant à un décalage en cents. */
    inline double centsToRatio (double cents) noexcept
    {
        return std::pow (2.0, cents / 1200.0);
    }

    /** Décalage en cents correspondant à un ratio de fréquence. */
    inline double ratioToCents (double ratio) noexcept
    {
        return 1200.0 * std::log2 (ratio);
    }

    /** Fréquence -> note MIDI flottante (69 = A4 = 440 Hz). */
    inline double hzToMidi (double hz) noexcept
    {
        return 69.0 + 12.0 * std::log2 (hz / 440.0);
    }

    /** Note MIDI -> fréquence. */
    inline double midiToHz (double midi) noexcept
    {
        return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0);
    }

    inline double dbToGain (double db) noexcept
    {
        return std::pow (10.0, db / 20.0);
    }

    /** Fenêtre de Hann symétrique de longueur l (utilisée par le moteur PSOLA). */
    inline double hann (double phase01) noexcept
    {
        return 0.5 - 0.5 * std::cos (2.0 * kPi * phase01);
    }
}
