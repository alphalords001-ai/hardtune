//  PitchDetector.cpp
#include "PitchDetector.h"
#include <cstring>

namespace tune
{

void PitchDetector::prepare (double sampleRate, int win, int hop)
{
    sr         = sampleRate;
    windowSize = std::max (256, win);
    hopSize    = std::max (32, hop);

    // La fenêtre doit contenir au moins ~2 périodes de la note la plus grave.
    // 2 périodes suffisent pour YIN ; avec 2.5 la fenêtre devenait si longue
    // (31 ms à 80 Hz) que la correction arrivait toujours en retard sur le
    // vibrato du chanteur : le vibrato n'était corrigé qu'à moitié.
    const int minUseful = (int) std::ceil (2.0 * sr / minHz);
    if (windowSize < minUseful)
        windowSize = std::min (4096, minUseful);

    tail.assign ((size_t) windowSize, 0.0f);
    carry.clear();
    carry.reserve ((size_t) hopSize * 8);
    diff.assign ((size_t) (windowSize + 2), 0.0);
    cmnd.assign ((size_t) (windowSize + 2), 0.0);
    absPos = 0;
}

void PitchDetector::reset()
{
    std::fill (tail.begin(), tail.end(), 0.0f);
    carry.clear();
    absPos = 0;
}

void PitchDetector::setRange (double lo, double hi)
{
    minHz = std::max (20.0, lo);
    maxHz = std::max (minHz * 1.2, hi);
}

void PitchDetector::process (const float* mono, int numSamples, std::vector<Estimate>& out)
{
    // Sans prepare(), `tail` est vide : le memmove ci-dessous partirait d'un
    // pointeur nul ("Read of address 0x0").
    if (mono == nullptr || numSamples <= 0)
        return;

    if (tail.size() < (size_t) windowSize || hopSize <= 0 || windowSize <= hopSize)
        return;

    carry.insert (carry.end(), mono, mono + numSamples);

    while ((int) carry.size() >= hopSize)
    {
        // 1. glisser le bloc dans la fenêtre glissante
        std::memmove (tail.data(), tail.data() + hopSize,
                      sizeof (float) * (size_t) (windowSize - hopSize));
        std::memcpy (tail.data() + (windowSize - hopSize), carry.data(),
                     sizeof (float) * (size_t) hopSize);

        carry.erase (carry.begin(), carry.begin() + hopSize);
        absPos += hopSize;

        // 2. analyser
        double hz = 0.0, conf = 0.0;
        if (analyse (hz, conf))
        {
            Estimate e;
            e.hz         = hz;
            e.confidence = conf;
            e.pos        = absPos;
            out.push_back (e);
        }
    }
}

bool PitchDetector::analyse (double& hzOut, double& confOut)
{
    //  ANALYSE SUR SIGNAL DECIME (facteur 2).
    //  La fonction de difference YIN coutait 513 x 512 = 262 000 operations en
    //  double par estimation, 375 fois par seconde : a elle seule ~24 % d'un
    //  coeur CPU, ce qui faisait deborder la deadline temps reel du DAW
    //  (craquements, ASIO Guard qui monte). On decime par 2 (moyenne de deux
    //  echantillons voisins, qui fait aussi office de filtre anti-repliement)
    //  et on calcule en float : meme duree d'analyse (meme nombre de
    //  millisecondes), quatre fois moins d'echantillons, instructions SIMD.
    const int Wd = windowSize / 2;

    if ((int) dec.size() < Wd)
        dec.assign ((size_t) Wd, 0.0f);

    const float* src = tail.data();

    for (int j = 0; j < Wd; ++j)
        dec[(size_t) j] = 0.5f * (src[2 * j] + src[2 * j + 1]);

    const double srDec = sr * 0.5;
    const int W = Wd;

    int tauMin = (int) std::floor (srDec / maxHz);
    int tauMax = (int) std::ceil  (srDec / minHz);
    tauMin = std::max (2, tauMin);
    tauMax = std::min (W / 2, tauMax);

    const int integ = W - tauMax;               // longueur d'intégration commune
    if (integ < 32 || tauMax <= tauMin + 2)
        return false;

    const float* x = dec.data();

    // --- fonction de différence (en float : vectorisable) -----------------------
    for (int tau = 0; tau <= tauMax; ++tau)
    {
        float sum = 0.0f;
        const float* a = x;
        const float* b = x + tau;

        for (int j = 0; j < integ; ++j)
        {
            const float d = a[j] - b[j];
            sum += d * d;
        }
        diff[(size_t) tau] = (double) sum;
    }

    // --- normalisation cumulative (CMNDF) --------------------------------------
    cmnd[0] = 1.0;
    double running = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        running += diff[(size_t) tau];
        cmnd[(size_t) tau] = (running > 1e-20) ? diff[(size_t) tau] * (double) tau / running
                                               : 1.0;
    }

    // --- seuil absolu -----------------------------------------------------------
    const double threshold = 0.15;
    int best = -1;

    for (int tau = tauMin; tau <= tauMax; ++tau)
    {
        if (cmnd[(size_t) tau] < threshold)
        {
            // descendre jusqu'au minimum local
            while (tau + 1 <= tauMax && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }

    if (best < 0)   // pas de candidat sous le seuil : prendre le minimum global
    {
        double bestVal = 1e9;
        for (int tau = tauMin; tau <= tauMax; ++tau)
            if (cmnd[(size_t) tau] < bestVal) { bestVal = cmnd[(size_t) tau]; best = tau; }

        if (best < 0 || bestVal > 0.45)
        {
            hzOut   = 0.0;
            confOut = 0.0;
            return true;      // on renvoie une estimation "non voisée"
        }
    }

    // --- interpolation parabolique ---------------------------------------------
    double tauRef = (double) best;
    if (best > tauMin && best < tauMax)
    {
        const double a = cmnd[(size_t) (best - 1)];
        const double b = cmnd[(size_t) best];
        const double c = cmnd[(size_t) (best + 1)];
        const double denom = (a - 2.0 * b + c);
        if (std::abs (denom) > 1e-20)
        {
            const double shift = 0.5 * (a - c) / denom;
            if (std::abs (shift) < 1.0)
                tauRef += shift;
        }
    }

    hzOut   = srDec / tauRef;
    confOut = std::max (0.0, std::min (1.0, 1.0 - cmnd[(size_t) best]));
    return true;
}

} // namespace tune
