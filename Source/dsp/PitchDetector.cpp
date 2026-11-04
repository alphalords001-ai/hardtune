//  PitchDetector.cpp — MPM / NSDF (voir PitchDetector.h).
#include "PitchDetector.h"

#include <cstring>

namespace tune
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    //  Seuil MPM : on retient le premier maximum local qui atteint 90 % du
    //  maximum global. C'est le k = 0,9 de McLeod. Plus bas (0,5) on descend
    //  d'une octave dès que la voix a une harmonique 2 forte ; plus haut (0,97)
    //  on rate le bon pic quand la voix est légèrement instable.
    constexpr double kMpmRatio = 0.90;

    //  Clarté minimale pour dire « voisé ». Mesuré sur la voix de référence :
    //  voyelles 0,85-0,98 ; « s » et souffle 0,05-0,25.
    constexpr double kVoicedClarity = 0.45;
}

// ---------------------------------------------------------------------------
void PitchDetector::buildDecimationFilter()
{
    //  Passe-bas de Butterworth approché par une sinc tronquée et fenêtrée
    //  (Blackman). Coupure à 0,22 x la fréquence d'échantillonnage décimée, soit
    //  ~4,4 kHz à 48 kHz : largement au-dessus du F0 et de ses 5 premières
    //  harmoniques, et bien sous le repliement (6 kHz).
    const double fc = 0.22 / (double) kDecim;   // cycles/échantillon réel
    const int    M  = kTaps - 1;

    double sum = 0.0;
    for (int i = 0; i < kTaps; ++i)
    {
        const double n = (double) i - 0.5 * (double) M;
        const double sinc = (std::abs (n) < 1e-9) ? 2.0 * fc
                                                  : std::sin (2.0 * kPi * fc * n) / (kPi * n);
        const double w = 0.42 - 0.5 * std::cos (2.0 * kPi * (double) i / (double) M)
                              + 0.08 * std::cos (4.0 * kPi * (double) i / (double) M);
        fir[i] = (float) (sinc * w);
        sum += (double) fir[i];
    }

    // gain unité en continu (indispensable : sinon le niveau décimé fausse le NSDF)
    if (std::abs (sum) > 1e-12)
        for (int i = 0; i < kTaps; ++i)
            fir[i] = (float) ((double) fir[i] / sum);
}

// ---------------------------------------------------------------------------
void PitchDetector::prepare (double sampleRate, int win, int hop)
{
    sr      = (sampleRate > 0.0 ? sampleRate : 48000.0);
    hopSize = std::max (32, hop);

    //  Fenêtre d'analyse : au moins 2 périodes de la note la plus grave
    //  (condition de validité du NSDF : tauMax <= nd/2). À 70 Hz et 48 kHz
    //  cela donne 172 échantillons décimés, donc nd = 352 (29 ms).
    const double scale = sr / (double) kDecim;
    tauMax = (int) std::ceil (scale / minHz);
    tauMin = (int) std::floor (scale / maxHz);
    tauMin = std::max (2, tauMin);
    tauMax = std::max (tauMin + 8, tauMax);

    nd = std::max (64, std::max (win, 256) / kDecim);
    nd = std::max (nd, 2 * tauMax + 8);

    windowSize = nd * kDecim;

    buildDecimationFilter();

    tail.assign ((size_t) windowSize + 2 * kTaps, 0.0f);
    dec.assign ((size_t) nd, 0.0f);
    nsdf.assign ((size_t) tauMax + 2, 0.0);
    powPrefix.assign ((size_t) nd + 2, 0.0);

    carryLen = 0;
    std::memset (carry, 0, sizeof (carry));
    absPos = 0;
}

void PitchDetector::reset()
{
    std::fill (tail.begin(), tail.end(), 0.0f);
    std::fill (dec.begin(), dec.end(), 0.0f);
    std::fill (nsdf.begin(), nsdf.end(), 0.0);
    std::fill (powPrefix.begin(), powPrefix.end(), 0.0);
    carryLen = 0;
    std::memset (carry, 0, sizeof (carry));
    absPos = 0;
}

void PitchDetector::setRange (double lo, double hi)
{
    minHz = std::max (20.0, lo);
    maxHz = std::max (minHz * 1.2, hi);

    // la plage a changé : les bornes de recherche doivent suivre
    if (! tail.empty())
    {
        const double scale = sr / (double) kDecim;
        tauMax = (int) std::ceil (scale / minHz);
        tauMin = std::max (2, (int) std::floor (scale / maxHz));
        tauMax = std::max (tauMin + 8, tauMax);
        tauMax = std::min (tauMax, nd / 2);
    }
}

// ---------------------------------------------------------------------------
PitchDetector::Estimate PitchDetector::process (const float* mono, int numSamples)
{
    Estimate e;
    if (mono == nullptr || numSamples <= 0 || tail.empty() || hopSize <= 0)
        return e;

    //  Niveau et passages par zéro sur le bloc courant : ce sont les indices des
    //  sons non harmoniques (sibilantes, plosives, souffle).
    double sq = 0.0;
    int    crossings = 0;
    float  prev = mono[0];
    for (int i = 0; i < numSamples; ++i)
    {
        const float x = mono[i];
        sq += (double) x * (double) x;
        if ((x > 0.0f) != (prev > 0.0f))
            ++crossings;
        prev = x;
    }

    e.rms = std::sqrt (sq / (double) numSamples);
    e.zcr = (double) crossings / (double) numSamples;
    e.pos = absPos;

    // --- alimentation de la fenêtre glissante, par pas de hopSize -------------
    int used = 0;
    while (used < numSamples)
    {
        const int room = hopSize - carryLen;
        const int take = std::min (room, numSamples - used);

        //  La fenêtre active est tail[kTaps .. kTaps+windowSize). Les nouveaux
        //  échantillons se placent à la fin de cette fenêtre.
        std::memcpy (tail.data() + (size_t) (kTaps + windowSize - hopSize + carryLen),
                     mono + used, sizeof (float) * (size_t) take);

        carryLen += take;
        used     += take;

        if (carryLen >= hopSize)
        {
            carryLen = 0;
            absPos  += hopSize;

            double hz = 0.0, clarity = 0.0;
            if (analyse (hz, clarity))
            {
                if (hz > 0.0 && clarity >= kVoicedClarity)
                {
                    e.hz         = hz;
                    e.confidence = clarity;
                    e.voiced     = true;
                    e.sibilant   = false;
                }
                else
                {
                    e.hz         = 0.0;
                    e.confidence = clarity;
                    e.voiced     = false;
                    //  Bruit non harmonique : beaucoup de passages par zéro et
                    //  aucune périodicité -> « S », « T », « P », souffle.
                    e.sibilant   = (e.zcr > 0.10);
                }
                e.valid = true;
                e.pos   = absPos;
            }

            //  Décalage de la fenêtre pour le pas suivant. Le dernier hopSize
            //  reste en place et sera écrasé par les prochains échantillons.
            std::memmove (tail.data() + kTaps, tail.data() + kTaps + hopSize,
                          sizeof (float) * (size_t) (windowSize - hopSize));
        }
    }

    return e;
}

// ---------------------------------------------------------------------------
bool PitchDetector::analyse (double& hzOut, double& confOut)
{
    if (tauMax <= tauMin + 4 || (int) dec.size() < nd)
        return false;

    const float* x = tail.data() + (size_t) kTaps;   // signal aligné sur la fenêtre

    // ---- 1. décimation par 4 (filtre FIR) ------------------------------------
    for (int j = 0; j < nd; ++j)
    {
        const int base = kDecim * j;
        float acc = 0.0f;
        for (int k = 0; k < kTaps; ++k)
            acc += fir[k] * x[base + k];
        dec[(size_t) j] = acc;
    }

    // ---- 2. retrait de la composante continue --------------------------------
    double mean = 0.0;
    for (int j = 0; j < nd; ++j)
        mean += (double) dec[(size_t) j];
    mean /= (double) nd;

    double energy = 0.0;
    for (int j = 0; j < nd; ++j)
    {
        const double v = (double) dec[(size_t) j] - mean;
        dec[(size_t) j] = (float) v;
        energy += v * v;
    }

    if (energy < 1e-12)
    {
        hzOut = 0.0; confOut = 0.0;
        return true;
    }

    // ---- 3. NSDF -------------------------------------------------------------
    //  r'(tau) = 2 * somme(x[j] x[j+tau]) / somme(x[j]^2 + x[j+tau]^2)
    //  Les sommes de carrés se lisent en O(1) grâce aux préfixes.
    powPrefix[0] = 0.0;
    for (int j = 0; j < nd; ++j)
    {
        const double v = (double) dec[(size_t) j];
        powPrefix[(size_t) (j + 1)] = powPrefix[(size_t) j] + v * v;
    }
    const double totalPow = powPrefix[(size_t) nd];

    const int maxLag = std::min (tauMax, nd - 2);

    for (int tau = 0; tau <= maxLag; ++tau)
    {
        const int n = nd - tau;

        float acc = 0.0f;
        const float* a = dec.data();
        const float* b = dec.data() + tau;
        for (int j = 0; j < n; ++j)
            acc += a[j] * b[j];

        const double den = (powPrefix[(size_t) n] - powPrefix[0])
                         + (totalPow - powPrefix[(size_t) tau]);

        nsdf[(size_t) tau] = (den > 1e-20) ? 2.0 * (double) acc / den : 0.0;
    }

    // ---- 4. sélection du pic (règle MPM) -------------------------------------
    double best = 0.0;
    for (int tau = tauMin; tau <= maxLag; ++tau)
        best = std::max (best, nsdf[(size_t) tau]);

    if (best < 0.30)
    {
        // aucune périodicité crédible
        hzOut = 0.0;
        confOut = std::max (0.0, best);
        return true;
    }

    const double threshold = kMpmRatio * best;
    int chosen = -1;

    for (int tau = tauMin; tau <= maxLag; ++tau)
    {
        const double r0 = nsdf[(size_t) tau];
        if (r0 < threshold)
            continue;

        const bool leftOk  = (tau <= tauMin) || (r0 >= nsdf[(size_t) (tau - 1)]);
        const bool rightOk = (tau >= maxLag) || (r0 >= nsdf[(size_t) (tau + 1)]);

        if (leftOk && rightOk)
        {
            chosen = tau;
            break;                       // premier pic = F0, pas l'octave en dessous
        }
    }

    if (chosen < 0)
        return true;   // hz reste à 0

    // ---- 5. interpolation parabolique sur le pic -----------------------------
    double tauPick = (double) chosen;
    if (chosen > tauMin && chosen < maxLag)
    {
        const double a = nsdf[(size_t) (chosen - 1)];
        const double b = nsdf[(size_t) chosen];
        const double c = nsdf[(size_t) (chosen + 1)];
        const double den = a - 2.0 * b + c;
        if (std::abs (den) > 1e-20)
        {
            const double shift = 0.5 * (a - c) / den;
            if (std::abs (shift) < 1.0)
                tauPick += shift;
        }
    }

    // ---- 6. affinage pleine bande -------------------------------------------
    //  À 12 kHz, un décalage de 60 (200 Hz) n'est précis qu'à ~3 cents. On
    //  recalcule le NSDF sur le signal réel autour de 4 x tauPick : la précision
    //  passe sous le cent, ce qui est nécessaire pour que les grains PSOLA
    //  restent en phase (sinon : « phasiness » et perte d'énergie).
    const double tauFull = refineLag (tauPick * (double) kDecim);

    if (tauFull < 2.0)
        return true;

    const double hz = sr / tauFull;
    if (hz < minHz * 0.9 || hz > maxHz * 1.1)
        return true;

    hzOut   = hz;
    confOut = std::max (0.0, std::min (1.0, nsdf[(size_t) chosen]));
    return true;
}

// ---------------------------------------------------------------------------
double PitchDetector::refineLag (double coarseLagFull) const
{
    const int w = windowSize;
    const int search = 4;

    const int centre = (int) std::lround (coarseLagFull);
    if (centre < 2 || centre + search + 2 >= w)
        return coarseLagFull;

    const float* x = tail.data() + (size_t) kTaps;

    double bestVal = -2.0;
    int    bestLag = centre;
    double vals[2 * 8 + 1] = { 0.0 };   // max 8 de portée

    for (int k = -search; k <= search; ++k)
    {
        const int lag = centre + k;
        const int n   = w - lag;

        double num = 0.0, e1 = 0.0, e2 = 0.0;
        for (int j = 0; j < n; ++j)
        {
            const double a = (double) x[j];
            const double b = (double) x[j + lag];
            num += a * b;
            e1  += a * a;
            e2  += b * b;
        }

        const double den = e1 + e2;
        const double v = (den > 1e-20) ? 2.0 * num / den : 0.0;
        vals[k + search] = v;

        if (v > bestVal)
        {
            bestVal = v;
            bestLag = lag;
        }
    }

    double refined = (double) bestLag;
    if (bestLag > centre - search && bestLag < centre + search)
    {
        const int idx = bestLag - centre + search;
        const double a = vals[idx - 1];
        const double b = vals[idx];
        const double c = vals[idx + 1];
        const double den = a - 2.0 * b + c;
        if (std::abs (den) > 1e-20)
        {
            const double shift = 0.5 * (a - c) / den;
            if (std::abs (shift) < 1.0)
                refined += shift;
        }
    }

    return refined;
}

} // namespace tune
