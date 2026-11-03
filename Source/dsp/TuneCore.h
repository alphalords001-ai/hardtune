//  TuneCore.h — moteur de correction de hauteur.
//
//  v2 : la synthèse est confiée à Rubber Band Library (moteur R3 « finer »,
//  fenêtre courte, préservation des formants). La détection de hauteur (YIN) et
//  la quantification sur la gamme restent maison.
//
//  Pourquoi ce changement : la première version utilisait un PSOLA maison qui
//  produisait des craquements et une modulation d'amplitude (« robot qui se vide
//  d'énergie »). Rubber Band est une bibliothèque de référence, utilisée entre
//  autres par Ardour, Bitwig et Sonic Visualiser : anti-clic natif, formants
//  préservés, latence annoncée de façon fiable.
//
//  Gestion des canaux : UN SEUL stretcher pour tous les canaux, ce qui garantit
//  que gauche et droite sont traitées exactement de la même façon (un signal
//  mono reste mono). L'ancienne version traitait les canaux séparément, d'où
//  l'effet stéréo parasite signalé au test.
//
//  Licence : Rubber Band est GPL v2+ ; ce projet est GPL v3, donc compatible.
#pragma once

#include <vector>
#include <atomic>
#include <memory>
#include <cstddef>

#include "PitchDetector.h"
#include "NoteMapper.h"
#include "TuneMath.h"

namespace RubberBand { class RubberBandStretcher; }

namespace tune
{

class TuneCore
{
public:
    struct Params
    {
        float amount      = 1.0f;    // 0 .. 1
        float retuneMs    = 0.0f;    // 0 .. 800 (0 = hard tune)
        float formant     = 0.0f;    // -1 .. +1  (throat)
        float mix         = 1.0f;    // 0 .. 1
        float outputGainDb= 0.0f;
        int   transpose   = 0;       // demi-tons
        bool  bypass      = false;

        // --- v2.1 : les deux réglages « humains » de l'interface ------------
        float humanize    = 0.0f;    // 0 .. 1  -> derive aléatoire lente (±15 cents)
        float vibrato     = 0.0f;    // 0 .. 1  -> vibrato ajouté (±50 cents)
        float vibratoHz   = 6.0f;    // 0.1 .. 12 Hz
    };

    TuneCore();
    ~TuneCore();

    void prepare (double sampleRate, int maximumBlockSize, int numChannels = 1);
    void reset();

    void setParams (const Params& p) noexcept  { params = p; }
    NoteMapper& notes() noexcept               { return mapper; }

    /** Traite un bloc (in et out peuvent pointer sur les mêmes canaux). */
    void process (const float* const* in, float* const* out, int numSamples, int numChannels);

    int latencySamples() const noexcept        { return latency; }

    // ---- informations pour l'interface -------------------------------------
    float getDetectedHz()   const noexcept { return uiHz.load(); }
    float getDetectedMidi() const noexcept { return uiMidi.load(); }
    int   getTargetNote()   const noexcept { return uiTarget.load(); }
    float getCentsError()   const noexcept { return uiCents.load(); }
    bool  isVoiced()        const noexcept { return uiVoiced.load(); }
    float getPitchRatio()   const noexcept { return uiRatio.load(); }

    // ---- vu-mètres (dB) ----------------------------------------------------
    float getInputPeakDb()  const noexcept { return uiInPeak.load(); }
    float getInputRmsDb()   const noexcept { return uiInRms.load(); }
    float getOutputPeakDb() const noexcept { return uiOutPeak.load(); }
    float getOutputRmsDb()  const noexcept { return uiOutRms.load(); }

private:
    struct Frame
    {
        long long pos = 0;
        double hz = 0.0;
        bool   voiced = false;
        double ratio = 1.0;
        int    target = -1;
    };

    void   pushFrame (long long pos, double hz, double conf);
    double currentRatio() const;
    double formantRatio() const;

    /** Traite une tranche de maxBlock samples au maximum (offset dans le bloc hôte). */
    void   processChunk (const float* const* in, float* const* out, int offset,
                         int numSamples, int channels);

    // --- configuration ------------------------------------------------------
    double sr = 48000.0;
    int    maxBlock = 512;
    int    numChannels = 1;
    std::atomic<bool> ready { false };   // prepare() a reussi : process() peut travailler
    Params params;

    // --- synthèse -----------------------------------------------------------
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;
    int latency = 0;

    std::vector<float> tmpWet;        // sortie brute du stretcher (maxBlock * canaux)
    std::vector<float> dryDelay;      // retard du chemin sec (canaux * taille)
    int dryMask = 0;
    long long writePos = 0;
    long long totalIn = 0;

    // --- détection ----------------------------------------------------------
    PitchDetector detector;
    std::vector<PitchDetector::Estimate> estimates;
    std::vector<Frame> frames;

    // --- quantification -----------------------------------------------------
    NoteMapper mapper;
    int    stickyNote = 60;
    bool   stickyValid = false;
    double smoothedRatio = 1.0;
    double lastGoodPeriod = 0.0;

    // --- humanize / vibrato -------------------------------------------------
    unsigned int rngState = 0x12345678u;
    double humanizeWalk  = 0.0;      // -1 .. +1, filtré
    double humanizeTarget= 0.0;      // -1 .. +1, tiré toutes les ~200 ms
    int    humanizeTicks = 0;
    double vibratoPhase  = 0.0;
    double humanizeCents ();

    // --- vu-mètres ----------------------------------------------------------
    double inPeakLin  = 0.0, inRmsLin  = 0.0;
    double outPeakLin = 0.0, outRmsLin = 0.0;
    static double linToDb (double x) noexcept;

    // --- état pour l'interface ----------------------------------------------
    std::atomic<float> uiHz { 0.0f };
    std::atomic<float> uiMidi { 0.0f };
    std::atomic<int>   uiTarget { -1 };
    std::atomic<float> uiCents { 0.0f };
    std::atomic<bool>  uiVoiced { false };
    std::atomic<float> uiRatio { 1.0f };

    std::atomic<float> uiInPeak  { -100.0f };
    std::atomic<float> uiInRms   { -100.0f };
    std::atomic<float> uiOutPeak { -100.0f };
    std::atomic<float> uiOutRms  { -100.0f };
};

} // namespace tune
