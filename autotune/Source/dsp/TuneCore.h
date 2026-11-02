//  TuneCore.h — moteur de correction de hauteur (cœur du plugin).
//
//  C++ pur, AUCUNE dépendance à JUCE : ce fichier est testé hors DAW par
//  tests/dsp_offline.cpp, puis utilisé tel quel par le plugin.
//
//  Chaîne de traitement :
//
//      entrée ─► tampon circulaire ─► détection YIN (toutes les 128 samples)
//             ─► quantification sur la gamme ─► lissage (retune speed)
//             ─► synthèse PSOLA (grains pitch-synchrones) + formants
//             ─► mélange dry/wet ─► gain ─► sortie
//
//  Latence : fixed, annoncée au DAW via latencySamples().
#pragma once

#include <vector>
#include <atomic>
#include <cstddef>

#include "PitchDetector.h"
#include "NoteMapper.h"
#include "TuneMath.h"

namespace tune
{

class TuneCore
{
public:
    struct Params
    {
        float amount      = 1.0f;    // 0 .. 1   (quantité de correction)
        float retuneMs    = 0.0f;    // 0 .. 800 (0 = hard tune instantané)
        float formant     = 0.0f;    // -1 .. 1  (throat / formant)
        float mix         = 1.0f;    // 0 .. 1
        float outputGainDb= 0.0f;    // -24 .. +12
        int   transpose   = 0;       // -12 .. +12 demi-tons
        bool  bypass      = false;
    };

    void prepare (double sampleRate, int maximumBlockSize);
    void reset();

    void setParams (const Params& p) noexcept     { params = p; }
    NoteMapper& notes() noexcept                  { return mapper; }

    /** Mode "esclave" : la détection est fournie de l'extérieur (canal droit). */
    void setExternalPitchMode (bool on) noexcept  { externalPitch = on; }
    void setExternalPitch (double hz, bool voiced) noexcept;

    /** Traite un bloc mono (in et out peuvent être le même pointeur). */
    void process (const float* in, float* out, int numSamples);

    int latencySamples() const noexcept           { return latency; }

    // ---- informations pour l'interface -------------------------------------
    float  getDetectedHz()   const noexcept { return uiHz.load(); }
    float  getDetectedMidi() const noexcept { return uiMidi.load(); }
    int    getTargetNote()   const noexcept { return uiTarget.load(); }
    float  getCentsError()   const noexcept { return uiCents.load(); }
    bool   isVoiced()        const noexcept { return uiVoiced.load(); }

private:
    struct Frame
    {
        long long pos = 0;      // position absolue de fin de fenêtre d'analyse
        double hz = 0.0;
        bool   voiced = false;
        double ratio = 1.0;     // ratio de correction lissé
    };

    void   pushFrame (long long pos, double hz, double conf);
    Frame  frameAt (long long pos) const;
    double periodAt (long long pos) const;
    double readInLinear (double pos) const;

    void   generateGrains();
    double findBestMark (double center, double period) const;
    void   emitSample (long long n, float& outSample);

    // --- configuration ------------------------------------------------------
    double sr = 48000.0;
    Params params;

    // --- tampons circulaires -------------------------------------------------
    static constexpr int kRingBits = 16;                  // 65536 samples (~1,5 s à 44,1 kHz)
    static constexpr int kRingSize = 1 << kRingBits;
    static constexpr int kRingMask = kRingSize - 1;

    std::vector<float> ringIn, ringWet, ringWsum;

    long long nIn = 0;       // samples d'entrée reçus
    long long nOut = 0;      // samples de sortie réellement produits
    int       latency = 0;   // = 2 périodes de la note la plus grave analysable

    // --- détection ----------------------------------------------------------
    PitchDetector detector;
    std::vector<PitchDetector::Estimate> estimates;
    std::vector<Frame> frames;
    bool   externalPitch = false;
    double extHz = 0.0;
    bool   extVoiced = false;

    // --- quantification -----------------------------------------------------
    NoteMapper mapper;
    int    stickyNote = 60;
    bool   stickyValid = false;
    double smoothedRatio = 1.0;

    // --- synthèse -----------------------------------------------------------
    double grainCenter = 0.0;       // centre (position absolue) du prochain grain
    double lastMark = 0.0;          // dernière marque de pitch utilisée (double !)
    bool   hasMark = false;
    double refinePeriod = 0.0;      // période au moment du dernier recalage
    bool   prevVoiced = false;
    double emitLimit = 0.0;         // on peut émettre jusque-là sans risque
    double lastGoodPeriod = 0.0;
    double lastVoicedWeight = 0.0;

    // --- paramètres dérivés -------------------------------------------------
    double Tmin = 40.0, Tmax = 800.0;   // bornes de période (samples)

    // --- état pour l'UI (atomique : lu par le thread GUI) --------------------
    std::atomic<float> uiHz { 0.0f };
    std::atomic<float> uiMidi { 0.0f };
    std::atomic<int>   uiTarget { -1 };
    std::atomic<float> uiCents { 0.0f };
    std::atomic<bool>  uiVoiced { false };
};

} // namespace tune
