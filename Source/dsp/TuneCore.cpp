//  TuneCore.cpp
#include "TuneCore.h"

#include <rubberband/RubberBandStretcher.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tune
{

namespace
{
    constexpr int    kHop = 128;                 // pas d'analyse
    constexpr double kVoicedThreshold = 0.30;

    constexpr double kFormantRangeCents = 500.0; // FORMANT ±100 % -> ±500 cents

    // Plage de transposition confiée à Rubber Band
    constexpr double kRatioMin = 0.5;
    constexpr double kRatioMax = 2.0;

    constexpr int kOptions =
          (int) RubberBand::RubberBandStretcher::OptionProcessRealTime
        | (int) RubberBand::RubberBandStretcher::OptionEngineFiner      // moteur R3
        | (int) RubberBand::RubberBandStretcher::OptionWindowShort      // latence minimale
        | (int) RubberBand::RubberBandStretcher::OptionPitchHighQuality
        | (int) RubberBand::RubberBandStretcher::OptionFormantPreserved
        | (int) RubberBand::RubberBandStretcher::OptionSmoothingOn
        // Canaux traites ENSEMBLE : sans cette option, Rubber Band elargit
        // artificiellement l'image stereo et casse la compatibilite mono
        // (c'est exactement le defaut signale : « le mono devient stereo »).
        | (int) RubberBand::RubberBandStretcher::OptionChannelsTogether;
}

// ============================================================================
double TuneCore::linToDb (double x) noexcept
{
    return 20.0 * std::log10 (std::max (1.0e-5, std::abs (x)));
}

// derive aléatoire lente, bornée à -1..+1 (marche filtrée)
double TuneCore::humanizeCents()
{
    // Une nouvelle cible toutes les ~200 ms : la voix derive lentement au lieu
    // de trembler. C'est exactement le comportement d'un chanteur humain.
    if (--humanizeTicks <= 0)
    {
        humanizeTicks = std::max (1, (int) std::lround (0.200 * sr / (double) kHop));
        rngState = rngState * 1664525u + 1013904223u;
        humanizeTarget = ((double) (rngState >> 8) / 16777216.0) * 2.0 - 1.0;
    }

    humanizeWalk += (humanizeTarget - humanizeWalk) * 0.05;
    return humanizeWalk;
}

// ============================================================================
TuneCore::TuneCore() = default;
TuneCore::~TuneCore() = default;

void TuneCore::prepare (double sampleRate, int maximumBlockSize, int channels)
{
    sr        = sampleRate;
    maxBlock  = std::max (32, maximumBlockSize);
    numChannels = std::max (1, std::min (2, channels));

    stretcher = std::make_unique<RubberBand::RubberBandStretcher> (
        (size_t) std::lround (sr),
        (size_t) numChannels,
        kOptions,
        1.0,        // time ratio (aucun changement de durée)
        1.0);       // pitch scale

    stretcher->setMaxProcessSize ((size_t) maxBlock);
    stretcher->setTimeRatio (1.0);
    stretcher->setPitchScale (1.0);
    stretcher->setFormantScale (1.0);

    latency = (int) stretcher->getLatency();

    detector.setRange (80.0, 1200.0);
    detector.prepare (sr, 1024, kHop);

    // retard du chemin sec = latence du stretcher, arrondie à une puissance de 2
    int size = 1;
    while (size < latency + maxBlock + 64) size <<= 1;
    dryMask = size - 1;
    dryDelay.assign ((size_t) size * (size_t) numChannels, 0.0f);

    tmpWet.assign ((size_t) maxBlock * 2 + 2, 0.0f);

    frames.clear();
    frames.reserve (512);
    estimates.reserve (64);

    reset();
}

void TuneCore::reset()
{
    if (stretcher != nullptr)
        stretcher->reset();

    std::fill (dryDelay.begin(), dryDelay.end(), 0.0f);
    writePos = 0;
    totalIn  = 0;

    detector.reset();
    frames.clear();
    estimates.clear();

    stickyValid   = false;
    stickyNote    = 60;
    smoothedRatio = 1.0;
    lastGoodPeriod = 0.0;

    humanizeWalk = 0.0;
    humanizeTarget = 0.0;
    humanizeTicks = 0;
    vibratoPhase = 0.0;
    rngState = 0x12345678u;

    inPeakLin = inRmsLin = outPeakLin = outRmsLin = 0.0;
    uiInPeak.store (-100.0f);
    uiInRms.store (-100.0f);
    uiOutPeak.store (-100.0f);
    uiOutRms.store (-100.0f);

    uiHz.store (0.0f);
    uiMidi.store (0.0f);
    uiTarget.store (-1);
    uiCents.store (0.0f);
    uiVoiced.store (false);
    uiRatio.store (1.0f);
}

// ============================================================================
//  Quantification
// ============================================================================
void TuneCore::pushFrame (long long pos, double hz, double conf)
{
    Frame f;
    f.pos    = pos;
    f.hz     = hz;
    f.voiced = (hz > 0.0 && conf > kVoicedThreshold);

    if (! f.voiced)
    {
        // Silence / consonne : on relâche la correction (ratio -> 1, transparent)
        smoothedRatio += (1.0 - smoothedRatio) * 0.25;
        f.ratio  = smoothedRatio;
        f.target = -1;
    }
    else
    {
        const double midiF = hzToMidi (hz);
        const int    cand  = mapper.nearestNote (midiF);

        if (! stickyValid)
        {
            stickyNote  = cand;
            stickyValid = true;
        }
        else if (cand != stickyNote)
        {
            const double dCand   = std::abs (midiF - (double) cand);
            const double dSticky = std::abs (midiF - (double) stickyNote);

            if (dSticky > 0.80 || dCand + 0.08 < dSticky)
                stickyNote = cand;
        }

        const double targetMidi = (double) (stickyNote + params.transpose);
        double cents = (targetMidi - midiF) * 100.0 * (double) params.amount;

        // HUMANIZE : petite dérive aléatoire lente (±15 cents max)
        if (params.humanize > 0.0001f)
            cents += humanizeCents() * (double) params.humanize * 15.0;

        // VIBRATO : oscillation ajoutée (±50 cents max)
        if (params.vibrato > 0.0001f)
        {
            const double hz = (double) std::max (0.1f, params.vibratoHz);
            vibratoPhase += 2.0 * 3.14159265358979 * hz * (double) kHop / sr;
            if (vibratoPhase > 6.283185307179586)
                vibratoPhase -= 6.283185307179586;
            cents += std::sin (vibratoPhase) * (double) params.vibrato * 50.0;
        }

        const double target = std::min (kRatioMax, std::max (kRatioMin, centsToRatio (cents)));

        double alpha = 0.0;
        if (params.retuneMs > 0.5f)
        {
            const double tau = (double) params.retuneMs * 0.001;
            alpha = std::min (0.9999, std::max (0.0, std::exp (-(double) kHop / (tau * sr))));
        }

        smoothedRatio = alpha * smoothedRatio + (1.0 - alpha) * target;
        f.ratio  = smoothedRatio;
        f.target = (int) std::lround (targetMidi);
    }

    frames.push_back (f);
    if (frames.size() > 600)
        frames.erase (frames.begin(), frames.begin() + 200);
}

double TuneCore::currentRatio() const
{
    return std::min (kRatioMax, std::max (kRatioMin, smoothedRatio));
}

double TuneCore::formantRatio() const
{
    return std::min (2.0, std::max (0.5,
             centsToRatio ((double) params.formant * kFormantRangeCents)));
}

// ============================================================================
//  Traitement
// ============================================================================
void TuneCore::process (const float* const* in, float* const* out, int numSamples, int channels)
{
    if (numSamples <= 0 || stretcher == nullptr)
        return;

    channels = std::max (1, std::min (2, channels));

    // ---- 1. chemin sec : on retarde l'entrée de la latence du stretcher -----
    for (int c = 0; c < channels; ++c)
    {
        const float* src = in[c];
        float* dst = dryDelay.data() + (size_t) c * (size_t) (dryMask + 1);
        for (int i = 0; i < numSamples; ++i)
            dst[(int) ((writePos + i) & dryMask)] = src[i];
    }

    // ---- 2. détection de hauteur (canal 1) ---------------------------------
    estimates.clear();
    detector.process (in[0], numSamples, estimates);

    for (const auto& e : estimates)
        pushFrame (e.pos, e.hz, e.confidence);

    totalIn += numSamples;

    // ---- 3. mise à jour du moteur ------------------------------------------
    stretcher->setPitchScale (currentRatio());
    stretcher->setFormantScale (formantRatio());

    // ---- 4. envoi au stretcher ---------------------------------------------
    const float* inPtrs[2] = { in[0], channels > 1 ? in[1] : in[0] };
    stretcher->process (inPtrs, (size_t) numSamples, false);

    // ---- 5. récupération ---------------------------------------------------
    int produced = 0;
    while (produced < numSamples)
    {
        const int avail = (int) stretcher->available();
        if (avail <= 0)
            break;

        const int take = std::min (avail, numSamples - produced);

        float* chans[2];
        for (int c = 0; c < channels; ++c)
            chans[c] = tmpWet.data() + (size_t) c * (size_t) maxBlock + (size_t) produced;

        stretcher->retrieve (chans, (size_t) take);
        produced += take;
    }

    // amorçage : les premiers `latency` samples n'existent pas encore
    for (int c = 0; c < channels; ++c)
        for (int i = produced; i < numSamples; ++i)
            tmpWet[(size_t) c * (size_t) maxBlock + (size_t) i] = 0.0f;

    // ---- 6. mélange sec / traité + gain ------------------------------------
    const double m = params.bypass ? 0.0 : (double) params.mix;
    const double g = dbToGain ((double) params.outputGainDb);

    for (int c = 0; c < channels; ++c)
    {
        const float* dry = dryDelay.data() + (size_t) c * (size_t) (dryMask + 1);
        const float* wet = tmpWet.data()  + (size_t) c * (size_t) maxBlock;

        for (int i = 0; i < numSamples; ++i)
        {
            const double d = (double) dry[(int) ((writePos + i - latency) & dryMask)];
            const double w = (double) wet[i];
            out[c][i] = (float) ((d + (w - d) * m) * g);
        }
    }

    writePos += numSamples;

    // ---- 6 bis. vu-mètres entrée / sortie ----------------------------------
    {
        double inPk = 0.0, inSq = 0.0, outPk = 0.0, outSq = 0.0;

        for (int c = 0; c < channels; ++c)
        {
            const float* src = in[c];
            for (int i = 0; i < numSamples; ++i)
            {
                const double x = (double) src[i];
                inPk = std::max (inPk, std::abs (x));
                inSq += x * x;

                const double y = (double) out[c][i];
                outPk = std::max (outPk, std::abs (y));
                outSq += y * y;
            }
        }

        const double n = (double) std::max (1, numSamples * channels);
        const double inRmsNow  = std::sqrt (inSq  / n);
        const double outRmsNow = std::sqrt (outSq / n);

        // attaque immédiate, relâchement doux (~300 ms)
        const double rel = std::exp (-(double) numSamples / (0.30 * sr));
        inPeakLin  = std::max (inPk,      inPeakLin  * rel);
        outPeakLin = std::max (outPk,     outPeakLin * rel);
        inRmsLin   = std::max (inRmsNow,  inRmsLin   * rel);
        outRmsLin  = std::max (outRmsNow, outRmsLin  * rel);

        uiInPeak.store  ((float) linToDb (inPeakLin));
        uiInRms.store   ((float) linToDb (inRmsLin));
        uiOutPeak.store ((float) linToDb (outPeakLin));
        uiOutRms.store  ((float) linToDb (outRmsLin));
    }

    // ---- 7. informations pour l'interface ----------------------------------
    if (! frames.empty())
    {
        const Frame& f = frames.back();
        if (f.voiced && f.hz > 0.0)
        {
            const double midiF = hzToMidi (f.hz);
            uiHz.store ((float) f.hz);
            uiMidi.store ((float) midiF);
            uiVoiced.store (true);
            uiTarget.store (f.target);
            if (f.target >= 0)
                uiCents.store ((float) ((midiF - (double) f.target) * 100.0));
            else
                uiCents.store (0.0f);
        }
        else
        {
            uiHz.store (0.0f);
            uiVoiced.store (false);
            uiTarget.store (-1);
            uiCents.store (0.0f);
        }
        uiRatio.store ((float) currentRatio());
    }
}

} // namespace tune
