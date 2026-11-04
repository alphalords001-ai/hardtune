//  PsolaEngine.cpp — TD-PSOLA (voir PsolaEngine.h pour le pourquoi).
#include "PsolaEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tune
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    //  Marge de sécurité ajoutée à la latence : la conversion double -> index
    //  et l'arrondi de la période ne doivent jamais faire lire un échantillon
    //  qui n'est pas encore arrivé.
    constexpr int kLatencyGuard = 24;

    int pow2AtLeast (int v)
    {
        int s = 1;
        while (s < v) s <<= 1;
        return s;
    }
}

// ---------------------------------------------------------------------------
void PsolaEngine::prepare (double sampleRate, int maximumBlockSize, int channels)
{
    sr = (sampleRate > 0.0 ? sampleRate : 48000.0);

    preparedChannels = std::max (1, std::min (2, channels));

    //  Tranche interne FIXE : c'est elle qui garantit que le résultat ne dépend
    //  pas de la taille de bloc du DAW (64, 512 ou 2048 donnent le même son).
    chunk = 64;

    pMax = sr / 70.0;      // 70 Hz : note la plus grave acceptée
    pMin = sr / 1000.0;    // 1000 Hz : note la plus aiguë

    //  Latence = 1,5 période max + deux tranches (voir la démonstration dans
    //  l'en-tête). À 48 kHz : 1029 + 128 = 1157 échantillons, soit 24,1 ms.
    //  Arrondie à un multiple de la tranche : sinon le premier bloc émis tombe
    //  au premier multiple de `chunk` qui suit, et la latence RÉELLE dépasse la
    //  latence ANNONCÉE (mesuré : 1216 au lieu de 1181). Le DAW ne compenserait
    //  alors qu'une partie du retard.
    const int raw = (int) std::ceil (1.5 * pMax) + 2 * chunk + kLatencyGuard;
    latency = ((raw + chunk - 1) / chunk) * chunk;

    //  L'anneau d'entrée doit contenir [emitPos - chunk, writePos), plus la
    //  marge d'un bloc hôte pour rester valide même si l'hôte envoie un bloc
    //  plus grand que ce que prepare() a annoncé.
    const int blockGuard = std::max (64, maximumBlockSize);
    histSize = pow2AtLeast (latency + chunk + blockGuard + (int) pMax + 64);
    histMask = histSize - 1;

    //  L'anneau de sortie doit contenir [emitPos - Hmax, emitPos + chunk + Hmax].
    const int hMax = (int) std::ceil (pMax * 1.6) + 2;
    olaSize = pow2AtLeast (chunk + 2 * hMax + 128);
    olaMask = olaSize - 1;

    hist.assign ((size_t) histSize * (size_t) preparedChannels, 0.0f);
    ola.assign  ((size_t) olaSize  * (size_t) preparedChannels, 0.0f);
    win.assign ((size_t) olaSize, 0.0f);

    reset();
}

void PsolaEngine::reset()
{
    std::fill (hist.begin(), hist.end(), 0.0f);
    std::fill (ola.begin(),  ola.end(),  0.0f);
    std::fill (win.begin(),  win.end(),  0.0f);

    writePos   = 0;
    emitPos    = 0;
    nextCenter = 0.0;
    srcPos     = 0.0;
    period     = sr / 220.0;      // valeur de départ neutre (A3)
    formantSm  = 1.0;
    mixSm      = 0.0;
}

// ---------------------------------------------------------------------------
double PsolaEngine::hann (double x) const
{
    if (x <= -1.0 || x >= 1.0)
        return 0.0;
    return 0.5 * (1.0 + std::cos (kPi * x));
}

float PsolaEngine::readHist (int channel, double pos) const
{
    if (pos < 0.0 || pos > (double) (writePos - 1))
        return 0.0f;

    const long long i0 = (long long) pos;
    const double    f  = pos - (double) i0;

    const float* base = hist.data() + (size_t) channel * (size_t) histSize;
    const float  a = base[(size_t) i0 & (size_t) histMask];
    const float  b = base[(size_t) (i0 + 1) & (size_t) histMask];

    return (float) ((1.0 - f) * (double) a + f * (double) b);
}

// ---------------------------------------------------------------------------
void PsolaEngine::writeGrain (long long center, double source, double p,
                              double halfWidth, int channels)
{
    //  Grain = fenêtre de Hann de largeur 2 x halfWidth à la sortie, lue dans une
    //  source de largeur 2 x p. Quand halfWidth == p, le grain est recopié à
    //  l'identique : les formants sont préservés (c'est le cas par défaut).
    //  Quand halfWidth != p, la source est comprimée ou étirée, ce qui déplace
    //  les formants sans toucher à la hauteur — c'est le réglage FORMANT.
    const int    hi   = (int) std::ceil (halfWidth);
    const double step = (halfWidth > 1e-9) ? (p / halfWidth) : 1.0;

    for (int j = -hi; j <= hi; ++j)
    {
        const double x = (double) j / halfWidth;
        const double w = hann (x);
        if (w <= 0.0)
            continue;

        const size_t idx = (size_t) (unsigned long long) (center + j) & (size_t) olaMask;

        for (int c = 0; c < channels; ++c)
        {
            const float s = readHist (c, source + (double) j * step);
            ola[(size_t) c * (size_t) olaSize + idx] += (float) (w * (double) s);
        }

        win[idx] += (float) w;
    }
}

// ---------------------------------------------------------------------------
void PsolaEngine::process (const float* const* in, float* const* out, int numSamples,
                           int channels, const Control& c)
{
    if (numSamples <= 0 || in == nullptr || out == nullptr)
        return;
    if (hist.empty() || ola.empty())
        return;

    const int ch = std::max (1, std::min (2, std::min (channels, preparedChannels)));

    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (chunk, numSamples - done);
        processChunk (in, out, done, n, ch, c);
        done += n;
    }

    //  Canaux non préparés (hôte qui change de bus) : on duplique le dernier
    //  canal traité plutôt que d'écrire hors des tampons.
    for (int c2 = ch; c2 < channels; ++c2)
        std::memcpy (out[c2], out[ch - 1], sizeof (float) * (size_t) numSamples);
}

void PsolaEngine::processChunk (const float* const* in, float* const* out, int offset,
                                int numSamples, int channels, Control c)
{
    // ---- 1. écriture de l'entrée dans l'anneau ------------------------------
    for (int c2 = 0; c2 < channels; ++c2)
    {
        const float* src = in[c2] + offset;
        float* dst = hist.data() + (size_t) c2 * (size_t) histSize;
        for (int i = 0; i < numSamples; ++i)
            dst[(size_t) (writePos + i) & (size_t) histMask] = src[i];
    }
    writePos += numSamples;

    // ---- 2. lissages (aucun saut de paramètre audible) -----------------------
    const double hopSec = (double) numSamples / sr;
    const double aFmt = std::exp (-hopSec / 0.020);
    const double aMix = std::exp (-hopSec / 0.005);

    const double fmtTarget = std::min (1.6, std::max (0.5, c.formant));
    formantSm = aFmt * formantSm + (1.0 - aFmt) * fmtTarget;
    mixSm     = aMix * mixSm     + (1.0 - aMix) * std::min (1.0, std::max (0.0, c.mix));

    //  Période : celle du détecteur, bornée. On la garde telle quelle (la lisser
    //  ferait dériver les grains hors phase) ; seule une valeur aberrante est
    //  rejetée.
    if (c.period >= pMin * 0.8 && c.period <= pMax * 1.2)
        period = std::min (pMax, std::max (pMin, c.period));

    const double ratio = std::min (2.0, std::max (0.5, c.ratio));

    // ---- 3. programme des grains jusqu'à couvrir la sortie demandée ---------
    //  Un grain de demi-largeur P couvre [c-P, c+P] : il suffit donc que le
    //  dernier grain programmé vérifie c + P >= emitPos + numSamples.
    const long long coverTarget = emitPos + numSamples;

    //  Garde-fou : si la source n'est pas encore arrivée (démarrage), on sort.
    int guard = 0;
    while (nextCenter + period < (double) coverTarget && guard++ < 64)
    {
        const double p = period;

        //  Recalage sur la période : dès que la source a dérivé d'une
        //  demi-période par rapport à la sortie, on saute ou on répète une
        //  période. C'est ce mécanisme — et lui seul — qui change la hauteur.
        const double drift = srcPos - nextCenter;
        if (drift > 0.5 * p)
            srcPos -= p;
        else if (drift < -0.5 * p)
            srcPos += p;

        if (srcPos + p > (double) writePos)
            break;                     // pas encore assez d'échantillons

        if (srcPos < 0.0)
        {
            //  Premier grain : source incomplète, on avance sans écrire.
            srcPos += p;
            nextCenter += p / ratio;
            continue;
        }

        //  Demi-largeur de sortie = P x échelle des formants, mais jamais moins
        //  que la moitié du pas des grains (sinon trou de couverture).
        const double step = p / ratio;
        const double halfWidth = std::max (p * formantSm, 0.55 * step);

        writeGrain ((long long) std::llround (nextCenter), srcPos, p, halfWidth, channels);

        srcPos     += p;
        nextCenter += step;
    }

    // ---- 4. sortie : un slot par échantillon, sec + (traité - sec) x mix ----
    //  Tant que la latence n'est pas remplie, on rend du silence : c'est le
    //  comportement attendu d'un plugin à latence annoncée (le DAW compense).
    const bool canEmit = (emitPos + numSamples) <= (writePos - (long long) latency);

    for (int c2 = 0; c2 < channels; ++c2)
    {
        const float* dryRing = hist.data() + (size_t) c2 * (size_t) histSize;
        float*       olaRing = ola.data()  + (size_t) c2 * (size_t) olaSize;
        float*       dst     = out[c2] + offset;

        if (! canEmit)
        {
            for (int i = 0; i < numSamples; ++i)
                dst[i] = 0.0f;
            continue;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const long long s  = emitPos + i;
            const size_t    si = (size_t) s & (size_t) olaMask;

            const double wsum = (double) win[(size_t) si];
            const double wet  = (wsum > 1e-6)
                                  ? (double) olaRing[si] / std::max (wsum, 0.35)
                                  : 0.0;
            const double dry  = (double) dryRing[(size_t) s & (size_t) histMask];

            dst[i] = (float) (dry + (wet - dry) * mixSm);

            olaRing[si] = 0.0f;
        }
    }

    if (canEmit)
    {
        //  la somme des fenêtres est commune à tous les canaux : on la remet à
        //  zéro une seule fois, sur la même plage.
        for (int i = 0; i < numSamples; ++i)
            win[(size_t) (emitPos + i) & (size_t) olaMask] = 0.0f;

        emitPos += numSamples;
    }
}

} // namespace tune
