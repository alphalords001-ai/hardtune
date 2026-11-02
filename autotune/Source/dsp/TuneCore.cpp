//  TuneCore.cpp
#include "TuneCore.h"

#include <cstring>
#include <cstdlib>

namespace tune
{

// ============================================================================
//  Constantes de conception
// ============================================================================
namespace
{
    constexpr double kRatioMin = 0.75;   // correction/transposition max vers le bas
    constexpr double kRatioMax = 1.50;   // correction/transposition max vers le haut

    constexpr int    kHop = 128;         // pas d'analyse (samples)
    constexpr double kVoicedThreshold = 0.30;

    // Facteur de latence = 2.8 périodes de la note la plus grave analysable.
    // Couvre la fenêtre de grain (2*max(T,H)) et la marge de génération.
    constexpr double kLatencyFactor = 2.5;

    constexpr double kFormantRangeCents = 400.0;  // formant ±1 -> ±400 cents
}

// ============================================================================
//  Cycle de vie
// ============================================================================
void TuneCore::prepare (double sampleRate, int /*maximumBlockSize*/)
{
    sr = sampleRate;

    Tmax = sr / 80.0;    // note la plus grave gérée : 80 Hz
    Tmin = sr / 1200.0;  // note la plus aiguë : 1200 Hz

    latency = (int) std::lround (kLatencyFactor * Tmax);

    ringIn.assign   ((size_t) kRingSize, 0.0f);
    ringWet.assign  ((size_t) kRingSize, 0.0f);
    ringWsum.assign ((size_t) kRingSize, 0.0f);

    detector.setRange (80.0, 1200.0);
    detector.prepare (sr, 1024, kHop);

    frames.clear();
    frames.reserve (512);
    estimates.reserve (64);

    reset();
}

void TuneCore::reset()
{
    std::fill (ringIn.begin(),   ringIn.end(),   0.0f);
    std::fill (ringWet.begin(),  ringWet.end(),  0.0f);
    std::fill (ringWsum.begin(), ringWsum.end(), 0.0f);

    detector.reset();
    frames.clear();
    estimates.clear();

    nIn = 0;
    nOut = 0;

    stickyValid   = false;
    stickyNote    = 60;
    smoothedRatio = 1.0;

    grainCenter = 0.0;
    lastMark = 0.0;
    hasMark = false;
    refinePeriod = 0.0;
    prevVoiced = false;
    emitLimit   = 0.0;
    lastGoodPeriod   = Tmax;
    lastVoicedWeight = 0.0;

    uiHz.store (0.0f);
    uiMidi.store (0.0f);
    uiTarget.store (-1);
    uiCents.store (0.0f);
    uiVoiced.store (false);
}

void TuneCore::setExternalPitch (double hz, bool voiced) noexcept
{
    extHz     = hz;
    extVoiced = voiced;
}

// ============================================================================
//  Quantification : note cible et ratio de correction
// ============================================================================
void TuneCore::pushFrame (long long pos, double hz, double conf)
{
    Frame f;
    f.pos    = pos;
    f.hz     = hz;
    f.voiced = (hz > 0.0 && conf > kVoicedThreshold);

    if (! f.voiced)
    {
        // Silence / consonne : on relâche la correction vers 1 (transparent)
        smoothedRatio += (1.0 - smoothedRatio) * 0.25;
        f.ratio = smoothedRatio;
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
            // Hystérésis : on ne change de note cible que si c'est clairement mieux,
            // sauf si l'écart devient énorme (nouvelle note franche).
            const double dCand   = std::abs (midiF - (double) cand);
            const double dSticky = std::abs (midiF - (double) stickyNote);

            if (dSticky > 0.80 || dCand + 0.08 < dSticky)
                stickyNote = cand;
        }

        const double targetMidi = (double) (stickyNote + params.transpose);
        double cents = (targetMidi - midiF) * 100.0;
        cents *= (double) params.amount;

        const double target = std::min (kRatioMax, std::max (kRatioMin, centsToRatio (cents)));

        // Lissage : "retune speed"
        double alpha = 0.0;
        if (params.retuneMs > 0.5f)
        {
            const double tau = (double) params.retuneMs * 0.001;
            alpha = std::exp (-(double) kHop / (tau * sr));
            alpha = std::min (0.9999, std::max (0.0, alpha));
        }

        smoothedRatio = alpha * smoothedRatio + (1.0 - alpha) * target;
        f.ratio = smoothedRatio;
    }

    frames.push_back (f);

    if (frames.size() > 600)
        frames.erase (frames.begin(), frames.begin() + 200);
}

TuneCore::Frame TuneCore::frameAt (long long pos) const
{
    if (frames.empty())
    {
        Frame f;
        f.pos = pos;
        return f;
    }

    // dernière frame dont la position est <= pos
    size_t lo = 0, hi = frames.size();
    while (lo < hi)
    {
        const size_t mid = (lo + hi) / 2;
        if (frames[mid].pos <= pos) lo = mid + 1;
        else                        hi = mid;
    }

    if (lo == 0) return frames.front();
    return frames[lo - 1];
}

double TuneCore::periodAt (long long pos) const
{
    Frame f = frameAt (pos);
    if (f.voiced && f.hz > 0.0)
    {
        const double T = sr / f.hz;
        return std::min (Tmax, std::max (Tmin, T));
    }
    return std::min (Tmax, std::max (Tmin, lastGoodPeriod));
}

// ============================================================================
//  Synthèse PSOLA
// ============================================================================
double TuneCore::readInLinear (double pos) const
{
    if (pos <= 0.0)
        return 0.0;

    long long p0 = (long long) std::floor (pos);
    double frac  = pos - (double) p0;

    const long long oldest = std::max<long long> (0, nIn - kRingSize + 2);
    if (p0 < oldest) { p0 = oldest; frac = 0.0; }
    if (p0 + 1 >= nIn)
        return ringIn[(int) (p0 & kRingMask)];

    const double a = ringIn[(int) (p0       & kRingMask)];
    const double b = ringIn[(int) ((p0 + 1) & kRingMask)];
    return a + (b - a) * frac;
}

void TuneCore::generateGrains()
{
    const double formantRatio = std::min (2.0,
                                   std::max (0.5, centsToRatio ((double) params.formant * kFormantRangeCents)));

    int guard = 0;

    while (guard++ < 256)
    {
        const long long approx = (long long) grainCenter;

        const double T = periodAt (approx);

        Frame f = frameAt (approx);
        if (f.voiced && f.hz > 0.0)
            lastGoodPeriod = T;

        double ratio = f.ratio;
        if (! (ratio > 0.01)) ratio = 1.0;
        ratio = std::min (kRatioMax, std::max (kRatioMin, ratio));

        const double H = T / ratio;             // pas de synthèse
        const double L = 2.0 * T;               // longueur du grain (2 périodes)

        // Entrée nécessaire : recalage de la marque (± T/4 + 1 période)
        // puis lecture du grain entier.
        const double need = 1.30 * T + std::max (0.0, 0.5 * L * (std::max (1.0, formantRatio) - 1.0)) + 4.0;
        if (grainCenter + need > (double) nIn)
            break;

        // --- marque de pitch -------------------------------------------------
        // On avance de période en période (T) ; quand l'avance accumulée
        // s'écarte de la position de lecture idéale, on duplique ou on saute
        // une période : c'est ce mécanisme qui produit le changement de hauteur.
        // Le recalage par corrélation n'a lieu qu'au début d'un passage voisé
        // ou si la période change franchement (évite les sauts de phase).
        const bool voicedOnset = (f.voiced && ! prevVoiced);
        const bool needRefine  = (! hasMark) || voicedOnset
                                 || (refinePeriod > 0.0
                                     && std::abs (T - refinePeriod) > 0.05 * refinePeriod);

        double mark;
        if (needRefine)
        {
            mark = findBestMark (grainCenter, T);
            refinePeriod = T;
        }
        else
        {
            double k = std::round ((grainCenter - lastMark) / T);
            if (k < 0.0) k = 0.0;
            mark = lastMark + k * T;
        }

        // On garantit |mark - centre| <= T/2 : le décalage temporel du chemin
        // "wet" reste ainsi borné à une demi-période.
        while (mark - grainCenter >  0.5 * T) mark -= T;
        while (mark - grainCenter < -0.5 * T) mark += T;

        lastMark  = mark;      // double : aucune erreur d'arrondi ne s'accumule
        hasMark   = f.voiced;
        prevVoiced = f.voiced;

        // --- écriture du grain ---------------------------------------------
        const long long c    = (long long) std::llround (grainCenter);
        const int       half = (int) std::llround (L * 0.5);

        for (int i = -half; i < half; ++i)
        {
            const double w = hann (((double) i + (double) half + 0.5) / ((double) half * 2.0));
            if (w <= 1e-4) continue;

            const double src = mark + (double) i * formantRatio;
            const double s   = readInLinear (src);

            const long long p = c + i;
            const int idx = (int) (p & kRingMask);

            ringWet[idx]  += (float) (s * w);
            ringWsum[idx] += (float) w;
        }

        grainCenter += H;

        const double limit = grainCenter - 0.5 * L - 1.0;
        if (limit > emitLimit)
            emitLimit = limit;
    }
}

// Recherche de la meilleure "marque de pitch" (début de période) autour de
// `center`, par corrélation normalisée avec la période précédente.
// C'est ce qui évite de couper la forme d'onde au milieu d'une impulsion
// glottique — la cause n°1 du son "granuleux" des PSOLA mal réglés.
double TuneCore::findBestMark (double center, double period) const
{
    const int Ti = (int) std::min (512.0, std::floor (period));
    if (Ti < 16)
        return center;

    const double lo = center - 0.5 * period;
    const double hi = center + 0.5 * period;

    double best = center, bestScore = -1.0e30;

    for (double q = lo; q <= hi; q += 1.0)
    {
        double score = 0.0;
        for (int j = 0; j < Ti; ++j)
            score += readInLinear (q + (double) j) * readInLinear (q + (double) j - period);

        if (score > bestScore)
        {
            bestScore = score;
            best = q;
        }
    }
    return best;
}

// ============================================================================
//  Sortie
// ============================================================================
void TuneCore::emitSample (long long n, float& outSample)
{
    const int idx = (int) (n & kRingMask);

    const float dry  = ringIn[idx];
    const float wsum = ringWsum[idx];

    float wet = dry;
    if (wsum > 0.15f)
        wet = ringWet[idx] / wsum;

    // poids "voisé" lissé : sur les consonnes et le souffle, on repasse en dry
    const Frame f = frameAt (n);
    const double targetW = f.voiced ? 1.0 : 0.0;
    const double coef = 1.0 / std::max (1.0, 0.004 * sr);
    lastVoicedWeight += (targetW - lastVoicedWeight) * coef;

    double m = (double) params.mix * lastVoicedWeight;
    if (params.bypass) m = 0.0;

    double y = (double) dry * (1.0 - m) + (double) wet * m;
    y *= dbToGain ((double) params.outputGainDb);

    outSample = (float) y;

    // libère la place dans l'anneau pour les samples futurs
    ringWet[idx]  = 0.0f;
    ringWsum[idx] = 0.0f;
}

void TuneCore::process (const float* in, float* out, int numSamples)
{
    if (numSamples <= 0)
        return;

    // ---- 1. écriture dans l'anneau ------------------------------------------
    for (int i = 0; i < numSamples; ++i)
        ringIn[(int) ((nIn + i) & kRingMask)] = in[i];

    // ---- 2. détection -------------------------------------------------------
    if (! externalPitch)
    {
        estimates.clear();
        detector.process (in, numSamples, estimates);

        for (const auto& e : estimates)
            pushFrame (e.pos, e.hz, e.confidence);
    }
    else
    {
        pushFrame (nIn + numSamples - 1, extVoiced ? extHz : 0.0, extVoiced ? 1.0 : 0.0);
    }

    nIn += numSamples;

    // ---- 3. synthèse --------------------------------------------------------
    if (! params.bypass || true)   // on synthétise toujours : la latence reste constante
        generateGrains();

    // ---- 4. sortie ----------------------------------------------------------
    int produced = 0;
    while (produced < numSamples)
    {
        if (nOut + latency <= nIn && (double) nOut < emitLimit)
        {
            float s = 0.0f;
            emitSample (nOut, s);
            out[produced++] = s;
            ++nOut;
        }
        else
        {
            // amorçage : les `latency` premiers samples de sortie sont du silence
            out[produced++] = 0.0f;
        }
    }

    // ---- 5. informations pour l'interface -----------------------------------
    const Frame f = frameAt (nOut);
    if (f.voiced && f.hz > 0.0)
    {
        const double midiF = hzToMidi (f.hz);
        uiHz.store ((float) f.hz);
        uiMidi.store ((float) midiF);
        uiVoiced.store (true);

        if (stickyValid)
        {
            uiTarget.store (stickyNote + params.transpose);
            const double targetMidi = (double) (stickyNote + params.transpose);
            uiCents.store ((float) ((midiF - targetMidi) * 100.0));
        }
    }
    else
    {
        uiHz.store (0.0f);
        uiVoiced.store (false);
        uiTarget.store (-1);
        uiCents.store (0.0f);
    }
}

} // namespace tune
