//  NoteMapper.h — gammes, tonalités et choix de la note cible.
//
//  Toute la logique "musicale" du plugin est ici : étant donné une hauteur
//  détectée, quelle est la note autorisée la plus proche ?
#pragma once

#include <array>
#include <cmath>
#include <string>

#include "TuneMath.h"

namespace tune
{

class NoteMapper
{
public:
    enum Scale
    {
        Chromatic = 0,
        Major,
        MinorNatural,
        MinorHarmonic,
        PentatonicMajor,
        PentatonicMinor,
        Blues,
        Dorian,
        NumScales
    };

    /** Masque de 12 bits : bit n = classe de hauteur n autorisée (0 = C). */
    static unsigned scaleMask (Scale s) noexcept
    {
        switch (s)
        {
            case Chromatic:        return 0xFFFu;                       // toutes
            case Major:            return 0xAB5u;                       // 0,2,4,5,7,9,11
            case MinorNatural:     return 0x5ADu;                       // 0,2,3,5,7,8,10
            case MinorHarmonic:    return 0x9ADu;                       // 0,2,3,5,7,8,11
            case PentatonicMajor:  return 0x295u;                       // 0,2,4,7,9
            case PentatonicMinor:  return 0x4A9u;                       // 0,3,5,7,10
            case Blues:            return 0x4A9u | 0x40u;               // + 6 (blue note)
            case Dorian:           return 0x6ADu;                       // 0,2,3,5,7,9,10
            default:               return 0xFFFu;
        }
    }

    static const char* scaleName (Scale s) noexcept
    {
        switch (s)
        {
            case Chromatic:       return "Chromatic";
            case Major:           return "Major";
            case MinorNatural:    return "Minor";
            case MinorHarmonic:   return "Harmonic Minor";
            case PentatonicMajor: return "Pentatonic Maj";
            case PentatonicMinor: return "Pentatonic Min";
            case Blues:           return "Blues";
            case Dorian:          return "Dorian";
            default:              return "?";
        }
    }

    static const char* noteName (int pitchClass) noexcept
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                         "F#", "G", "G#", "A", "A#", "B" };
        return names[((pitchClass % 12) + 12) % 12];
    }

    void setKey (int pitchClass) noexcept { key = ((pitchClass % 12) + 12) % 12; }
    int  getKey() const noexcept          { return key; }

    void setScale (Scale s) noexcept      { scale = s; useCustom = false; }
    Scale getScale() const noexcept       { return scale; }

    void setCustomMask (unsigned m) noexcept { customMask = m & 0xFFFu; useCustom = true; }
    unsigned getCustomMask() const noexcept  { return customMask; }
    bool isCustom() const noexcept           { return useCustom; }

    /** Masque absolu (déjà transposé par la tonalité). */
    unsigned activeMask() const noexcept
    {
        const unsigned base = useCustom ? customMask : scaleMask (scale);
        return base;
    }

    /** Vrai si la classe de hauteur (absolue, 0 = C) est autorisée. */
    bool isPitchClassAllowed (int pitchClass) const noexcept
    {
        const int rel = (((pitchClass - key) % 12) + 12) % 12;
        return (activeMask() & (1u << rel)) != 0u;
    }

    /**
        Note MIDI autorisée la plus proche de `midiFloat`.
        Recherche d'abord la même note, puis ±1, ±2 … demi-tons.
    */
    int nearestNote (double midiFloat) const noexcept
    {
        const int base = (int) std::lround (midiFloat);
        for (int d = 0; d <= 6; ++d)
        {
            if (isPitchClassAllowed (base - d)) return base - d;
            if (isPitchClassAllowed (base + d)) return base + d;
        }
        return base;   // chromatique : ne devrait jamais arriver
    }

private:
    int      key = 0;
    Scale    scale = Major;
    unsigned customMask = 0xFFFu;
    bool     useCustom = false;
};

} // namespace tune
