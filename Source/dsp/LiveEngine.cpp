//  LiveEngine.cpp — ligne à retard à lecture pitch-synchrone (voir LiveEngine.h).
#include "LiveEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tune
{

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    int pow2AtLeast (int v)
    {
        int s = 1;
        while (s < v) s <<= 1;
        return s;
    }

    //  Constante de temps du lissage du ratio. Elle ne sert qu'à supprimer les
    //  marches entre deux tranches de traitement (64 échantillons = 1,3 ms) :
    //  elle doit donc rester très courte devant le temps de correction musical
    //  (retune), sinon les deux lissages se cumulent et la correction traîne.
    constexpr double kRatioSmoothingSec = 0.003;
}

// ---------------------------------------------------------------------------
void LiveEngine::prepare (double sampleRate, int maximumBlockSize, int channels)
{
    sr = (sampleRate > 0.0 ? sampleRate : 48000.0);
    preparedChannels = std::max (1, std::min (2, channels));

    pMax = sr / 70.0;      // 70 Hz : note la plus grave acceptée
    pMin = sr / 1000.0;    // 1000 Hz : note la plus aiguë

    maxPeriod      = (int) std::ceil (pMax);
    hopSize        = std::max (32, (int) std::lround (sr / 375.0));
    unvoicedPeriod = (int) std::lround (sr / 200.0);

    //  Latence = petit retard de sécurité + 1 / 400 s. À 44,1 kHz : 8,8 + 110 =
    //  119 échantillons (2,70 ms) ; à 48 kHz : 9,6 + 120 = 130 (2,71 ms). Le
    //  point de lecture se promène ensuite entre ce retard et ce retard plus une
    //  période (voir maybeSplice) : c'est le prix du mode sans avance.
    minDelay = std::max (8.0, std::round (sr * 0.0002));
    latency  = (int) std::lround (minDelay + sr / 400.0);

    peakDecay  = std::exp (-1.0 / sr);                        // ~1 s de mémoire
    centsCoeff = 1.0 - std::exp (-1.0 / (kRatioSmoothingSec * sr));

    //  L'anneau doit contenir [retard minimal, retard maximal + une période de
    //  recherche vers l'avant] plus une marge de bloc.
    const int blockGuard = std::max (64, maximumBlockSize);
    histSize = pow2AtLeast (latency + (int) (3.5 * pMax) + blockGuard + 1024);
    histMask = histSize - 1;

    hist.assign ((size_t) histSize * (size_t) preparedChannels, 0.0f);

    reset();
}

void LiveEngine::reset()
{
    std::fill (hist.begin(), hist.end(), 0.0f);

    writePos = 0;

    //  Les points de lecture sont en positions ABSOLUES, donc négatifs tant que
    //  la latence n'est pas remplie : c'est ce qui produit le silence du début
    //  (readHist renvoie zéro) puis un retard exactement égal à `latency`.
    //  Les laisser à zéro ferait sortir l'entrée sans retard et collerait le
    //  point de lecture à la limite d'écriture.
    tap       = -(double) latency;
    dryTap    = -(double) latency;
    spliceTap = -(double) latency;

    cents     = 0.0;
    fadeLength = 0;
    fadePosition = 0;
    fadeCorrelation = 1.0;

    trackedPeriod = sr / 220.0;
    unvoicedRun = 0;
    quietRun = 0;
    longPeak = 0.0;

    std::fill (trackPeriod, trackPeriod + kTrackHops, 0.0);
    std::fill (trackVoiced, trackVoiced + kTrackHops, false);
    trackPos = 0;
    trackCount = 0;

    splices = 0;
    driftBacks = 0;
}

// ---------------------------------------------------------------------------
float LiveEngine::readHist (int channel, double pos) const
{
    //  « Écrit » s'arrête à writePos - 1 ; l'interpolation demande i0 + 1, donc
    //  la dernière position lisible est writePos - 2. Lire au-delà renverrait
    //  des échantillons pas encore arrivés : on renvoie zéro et les appels
    //  bornent leur point de lecture bien avant.
    if (pos < 0.0 || pos > (double) (writePos - 2))
        return 0.0f;

    const long long i0 = (long long) pos;
    const double    f  = pos - (double) i0;

    const float* base = hist.data() + (size_t) channel * (size_t) histSize;
    const float  a = base[(size_t) i0 & (size_t) histMask];
    const float  b = base[(size_t) (i0 + 1) & (size_t) histMask];

    return (float) ((1.0 - f) * (double) a + f * (double) b);
}

// ---------------------------------------------------------------------------
double LiveEngine::periodicity (double period) const
{
    //  Meilleure corrélation normalisée du son le plus récent avec lui-même une
    //  période (±10 %) plus tôt. 1 = parfaitement périodique, 0 = bruit. Sert à
    //  distinguer un TROU dans une note tenue (périodique) d'une vraie consonne
    //  (non périodique) avant de ramener le point de lecture à la latence.
    if (period < 8.0)
        return 0.0;

    const int w = std::min (512, std::max (32, (int) std::lround (period)));
    const int lo = std::max (2, (int) std::floor (0.9 * period));
    const int hi = (int) std::ceil (1.1 * period);

    const float* x = hist.data();     // décision prise sur le canal 0

    double ref = 0.0;
    for (int i = 0; i < w; ++i)
    {
        const float a = x[(size_t) (writePos - 1 - i) & (size_t) histMask];
        ref += (double) a * (double) a;
    }

    if (ref < 1.0e-12)
        return 0.0;

    double best = 0.0;

    for (int lag = lo; lag <= hi; ++lag)
    {
        double cross = 0.0, energy = 0.0;

        for (int i = 0; i < w; ++i)
        {
            const double a = (double) x[(size_t) (writePos - 1 - i) & (size_t) histMask];
            const double b = (double) x[(size_t) (writePos - 1 - i - lag) & (size_t) histMask];
            cross += a * b;
            energy += b * b;
        }

        if (energy > 1.0e-12)
            best = std::max (best, cross / std::sqrt (ref * energy));
    }

    return best;
}

// ---------------------------------------------------------------------------
double LiveEngine::findSpliceLength (double tapPos, int direction, double minLength,
                                     double maxLength, double period, double& correlation) const
{
    //  On compare le cycle juste derrière le point de lecture avec celui situé
    //  `length` échantillons plus loin (direction -1 : dans le passé, +1 : vers
    //  le présent) et on garde la longueur où les deux se ressemblent le plus.
    correlation = 1.0;

    const int window = std::min (512, std::max (32, (int) std::lround (period)));
    const long long base = (long long) std::floor (tapPos);
    const int lo = (int) std::ceil (minLength);
    const int hi = (int) std::floor (maxLength);

    if (hi <= lo)
        return 0.5 * (minLength + maxLength);

    const float* x = hist.data();

    double reference = 0.0;
    for (int i = 0; i < window; ++i)
    {
        const float a = x[(size_t) (base - i) & (size_t) histMask];
        reference += (double) a * (double) a;
    }

    if (reference < 1.0e-12)
    {
        correlation = 0.0;
        return 0.5 * (minLength + maxLength);
    }

    auto similarity = [&] (int length)
    {
        double cross = 0.0, energy = 0.0;
        const long long other = base + (long long) direction * (long long) length;

        for (int i = 0; i < window; ++i)
        {
            const double a = (double) x[(size_t) (base  - i) & (size_t) histMask];
            const double b = (double) x[(size_t) (other - i) & (size_t) histMask];
            cross  += a * b;
            energy += b * b;
        }

        return cross / std::sqrt (reference * energy + 1.0e-20);
    };

    int    bestLength = lo;
    double bestScore  = -2.0;

    auto consider = [&] (int length)
    {
        const double score = similarity (length);

        if (score > bestScore)
        {
            bestScore  = score;
            bestLength = length;
        }
    };

    //  La plage peut couvrir plus d'un millier de longueurs après un saut vers
    //  le grave : un premier balayage tous les 2 échantillons, puis chaque
    //  longueur autour du gagnant.
    const int stride = std::max (1, (int) std::lround (sr / 24000.0));

    for (int length = lo; length <= hi; length += stride)
        consider (length);

    const int coarse = bestLength;

    for (int length = std::max (lo, coarse - stride + 1); length <= std::min (hi, coarse + stride - 1); ++length)
        if (length != coarse)
            consider (length);

    correlation = std::max (-1.0, std::min (1.0, bestScore));

    //  Raffinement sous-échantillon par parabole.
    if (bestLength > lo && bestLength < hi)
    {
        const double a = similarity (bestLength - 1), b = bestScore, c = similarity (bestLength + 1);
        const double curvature = a - 2.0 * b + c;

        if (curvature < 0.0)
            return (double) bestLength + std::max (-0.5, std::min (0.5, 0.5 * (a - c) / curvature));
    }

    return (double) bestLength;
}

// ---------------------------------------------------------------------------
void LiveEngine::startSplice (double offset, double fadeSamples, double correlation)
{
    spliceTap       = tap + offset;
    fadeLength      = std::max (1, (int) std::lround (fadeSamples));
    fadePosition    = 0;
    fadeCorrelation = std::max (0.0, std::min (1.0, correlation));

    if (correlation > 0.0) ++splices; else ++driftBacks;
}

// ---------------------------------------------------------------------------
bool LiveEngine::driftBackInaudible (double delay) const
{
    //  Les deux copies ne sont pas séparées d'un nombre entier de périodes : sur
    //  un son tenu, le fondu est un court « bop » filtré en peigne. Il ne
    //  s'entend pas si tout ce qu'il va mélanger, du point de lecture le plus
    //  ancien jusqu'au plus récent, est proche du silence…
    if ((double) quietRun > std::max (delay, (double) latency) + 2.0)
        return true;

    //  … ou si c'est du bruit. Le suivi lit « non voisé » pendant quelques ms
    //  quand une note en remplace une autre, et dans les trous de moins de
    //  20 ms : on attend donc ce délai, et pas pendant que le niveau monte (une
    //  note qui démarre) ni quand le son se répète à la dernière période (un
    //  trou plus long, donc toujours la même note). Vérifié une fois par hop.
    if (writePos % hopSize != 0 || std::abs (delay - (double) latency) <= 0.25 * unvoicedPeriod)
        return false;

    if (unvoicedRun < (int) std::lround (0.02 * sr))
        return false;

    const float* x = hist.data();
    const int window = (int) std::lround (0.01 * sr);

    double recent = 0.0, before = 0.0;
    for (int i = 0; i < window; ++i)
    {
        const double a = (double) x[(size_t) (writePos - 1 - i) & (size_t) histMask];
        const double b = (double) x[(size_t) (writePos - 1 - window - i) & (size_t) histMask];
        recent += a * a;
        before += b * b;
    }

    return recent <= 2.0 * before && periodicity (trackedPeriod) < 0.6;
}

// ---------------------------------------------------------------------------
void LiveEngine::maybeSplice (double ratio, double period, bool voiced)
{
    const double newest = (double) (writePos - 1);
    const double delay  = newest - tap;
    const bool inRange  = delay < minDelay + 1.5 * (double) maxPeriod;

    if (voiced && period > 1.0 && inRange)
    {
        //  Lire plus vite que l'écriture (hauteur vers le haut) finit par
        //  rattraper le présent : on saute UN cycle en arrière et on le répète.
        //  Lire moins vite (vers le bas) prend du retard : on saute un cycle en
        //  avant et on l'omet. Le saut vaut exactement une période du signal :
        //  il est donc inaudible, et la recherche par corrélation trouve cette
        //  période au point de lecture.
        double fade = std::max (16.0, std::min (0.006 * sr, 0.5 * period));

        if (std::abs (ratio - 1.0) > 1.0e-9)
            fade = std::min (fade, 0.9 * period / std::abs (ratio - 1.0));

        //  Le détecteur peut s'accrocher à une harmonique (un tiers ou un quart
        //  de la période) pendant quelques hops. Mesuré contre elle, le retard
        //  paraît trop long et se fait couper par une rafale de sauts courts qui
        //  ne tombent pas sur le vrai cycle : on juge le retard et on cherche
        //  avec la plus longue période des 24 derniers hops (~64 ms).
        auto longestRecentPeriod = [this, period]
        {
            double longest = period;

            for (int i = std::max (0, trackCount - 24); i < trackCount; ++i)
            {
                const int slot = (trackPos - trackCount + i) & (kTrackHops - 1);

                if (trackVoiced[slot])
                    longest = std::max (longest, trackPeriod[slot]);
            }

            return longest;
        };

        //  GARDE-FOU ABSOLU (filet de securite). Le point de lecture ne doit
        //  jamais atteindre la tete d'ecriture : il s'y bloquerait (le son se
        //  fige sur un echantillon) puis repartirait d'un coup — c'est
        //  exactement le defaut qu'on entend comme une microcoupure.
        //  Le seuil normal de saut est a ~11 echantillons de retard ; celui-ci
        //  n'est arme qu'a 4, c'est-a-dire quand la butee est imminente. Il ne
        //  se declenche donc pas en marche normale : verifie sur 21,5 s de voix
        //  reelle, le nombre d'evenements reste identique avec ou sans lui.
        if (ratio > 1.0 && delay < 4.0)
        {
            double correlation = 1.0;
            const double longest = longestRecentPeriod();
            const double length = findSpliceLength (tap, -1, 0.8 * period,
                                                    1.25 * longest, period, correlation);
            startSplice (-length, fade, correlation);
            return;
        }

        if (ratio > 1.0 && delay < minDelay + (ratio - 1.0) * fade + 2.0)
        {
            double correlation = 1.0;
            const double longest = longestRecentPeriod();
            const double length = findSpliceLength (tap, -1, 0.8 * period, 1.25 * longest, longest, correlation);

            startSplice (-length, fade, correlation);
            return;
        }

        if (ratio < 1.0 && delay > minDelay + 1.25 * period + 2.0)
        {
            const double longest = longestRecentPeriod();

            if (delay > minDelay + 1.25 * longest + 2.0)
            {
                double correlation = 1.0;
                const double maxLength = std::min (1.25 * longest, delay - minDelay - 2.0);
                const double length = findSpliceLength (tap, 1, 0.8 * period, maxLength, longest, correlation);
                startSplice (length, fade, correlation);
            }
        }

        return;
    }

    //  Aucune hauteur à suivre : on revient à la latence annoncée pour que le
    //  mixage et la compensation du DAW restent alignés (tout de suite si le
    //  retard a quitté la plage que les sauts maintiennent). Le son non voisé
    //  n'est pas corrélé : fondu à puissance constante. Le fondu n'excède pas le
    //  retard, donc il ne mélange que de l'audio déjà arrivé.
    const double offset = delay - (double) latency;

    if (std::abs (offset) > 0.5 && (! inRange || driftBackInaudible (delay)))
        startSplice (offset, std::max (16.0, std::min (delay, (double) latency)), 0.0);
}

// ---------------------------------------------------------------------------
void LiveEngine::process (const float* const* in, float* const* out, int numSamples,
                          int channels, const Control& c)
{
    if (numSamples <= 0 || in == nullptr || out == nullptr)
        return;
    if (hist.empty() || histSize <= 0)
        return;

    const int ch = std::max (1, std::min (2, std::min (channels, preparedChannels)));

    //  Tranche interne fixe : le résultat ne doit pas dépendre de la taille de
    //  bloc du DAW.
    constexpr int kChunk = 64;

    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (kChunk, numSamples - done);
        processChunk (in, out, done, n, ch, c);
        done += n;
    }

    for (int c2 = ch; c2 < channels; ++c2)
        std::memcpy (out[c2], out[ch - 1], sizeof (float) * (size_t) numSamples);
}

void LiveEngine::processChunk (const float* const* in, float* const* out, int offset,
                               int numSamples, int channels, const Control& c)
{
    const double ratioTarget  = std::max (0.5, std::min (2.0, c.ratio));
    const double period       = (c.period >= pMin * 0.8 && c.period <= pMax * 1.2)
                                    ? std::max (pMin, std::min (pMax, c.period))
                                    : trackedPeriod;
    const bool   voiced       = c.voiced && c.period > 1.0;
    //  v0.8.3 — LE MÉLANGE SEC/TRAITÉ NE DOIT JAMAIS SAUTER.
    //  Le mixage valait 0 tant que le détecteur n'entendait pas de hauteur, puis
    //  1 dès la première trame voisée : un saut de 0 à 1 en un seul bloc à
    //  chaque attaque de note. Or le traité est décalé d'une quarantaine
    //  d'échantillons par rapport au sec (le point de lecture courant n'est pas
    //  à la latence). Passer de l'un à l'autre d'un coup, c'est une marche dans
    //  le signal — le craquement.
    //  Mesuré sur un signal synthétique parfaitement propre (60 s, 0 craquement
    //  en entrée) : 10 craquements en sortie, dont un saut de 11 % de l'amplitude.
    //  Avec ce fondu de 12 ms : 0 craquement, saut maximal 0.
    const double mixTargetNow = std::max (0.0, std::min (1.0, c.mix));
    const double mixCoeff     = 1.0 - std::exp (-1.0 / (0.012 * sr));

    //  Le ratio ne s'applique qu'à un son voisé. Sur une consonne, une plosive
    //  ou un souffle il n'y a pas de hauteur à corriger : on lit à la vitesse de
    //  l'entrée, comme le signal original. C'est aussi ce qui permet au point de
    //  lecture de revenir à la latence annoncée sans déformer le bruit.
    const double centsTarget = voiced ? 1200.0 * std::log2 (ratioTarget) : 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        mixNow += (mixTargetNow - mixNow) * mixCoeff;

        // ---- 1. écriture de l'échantillon d'entrée ---------------------------
        for (int ch = 0; ch < channels; ++ch)
            hist[(size_t) ch * (size_t) histSize + ((size_t) writePos & (size_t) histMask)]
                = in[ch][offset + i];

        ++writePos;

        //  Pas de garde de démarrage : les points de lecture sont négatifs au
        //  début et readHist renvoie zéro de lui-même, ce qui produit le silence
        //  de la latence. Une garde qui sortirait de la boucle AVANT d'avancer
        //  les points de lecture décalerait tout le reste du signal (mesuré :
        //  133 échantillons au lieu de 130).
        const double newest = (double) (writePos - 1);

        // ---- 2. niveau et silence ------------------------------------------
        {
            const double level = std::abs ((double) hist[(size_t) (writePos - 1) & (size_t) histMask]);
            longPeak = std::max (level, longPeak * peakDecay);
            const double floorLevel = std::max (1.0e-4, 0.003 * longPeak);

            quietRun = (level < floorLevel) ? std::min (quietRun + 1, histSize) : 0;
            unvoicedRun = voiced ? 0 : std::min (unvoicedRun + 1, histSize);
        }

        if (voiced)
            trackedPeriod = period;

        // ---- 3. historique des périodes (une entrée par hop) ----------------
        if (writePos % hopSize == 0)
        {
            trackPeriod[trackPos] = (double) (float) trackedPeriod;
            trackVoiced[trackPos] = voiced;
            trackPos = (trackPos + 1) & (kTrackHops - 1);
            trackCount = std::min (trackCount + 1, kTrackHops);
        }

        // ---- 4. lissage du ratio -------------------------------------------
        cents += (centsTarget - cents) * centsCoeff;
        const double ratioNow = std::exp2 (cents / 1200.0);

        // ---- 5. saut de cycle si la dérive l'exige --------------------------
        if (fadeLength == 0)
            maybeSplice (ratioNow, trackedPeriod, voiced);

        // ---- 6. lecture -----------------------------------------------------
        const double limit = newest - 1.0;   // readHist exige pos <= writePos - 2

        double wet[2] = { 0.0, 0.0 };

        if (fadeLength > 0)
        {
            //  Fondu en cosinus surélevé : il conserve le niveau de deux copies
            //  qui coïncident (cycles entiers d'un son tenu) mais peut creuser
            //  de 3 dB là où elles ne coïncident pas (bruit, ou cycle qui a
            //  changé). On le renormalise en puissance avec la corrélation que
            //  la recherche a mesurée, pour qu'un saut dans une voix qui bouge ne
            //  laisse pas un trou.
            const double t = ((double) fadePosition + 0.5) / (double) fadeLength;
            double fadeIn  = 0.5 - 0.5 * std::cos (kPi * t);
            double fadeOut = 1.0 - fadeIn;
            const double power = fadeIn * fadeIn + fadeOut * fadeOut
                               + 2.0 * fadeCorrelation * fadeIn * fadeOut;

            if (power > 1.0e-12)
            {
                fadeIn  /= std::sqrt (power);
                fadeOut /= std::sqrt (power);
            }

            for (int ch = 0; ch < channels; ++ch)
                wet[ch] = fadeOut * (double) readHist (ch, tap)
                        + fadeIn  * (double) readHist (ch, spliceTap);

            spliceTap += ratioNow;

            if (++fadePosition >= fadeLength)
            {
                tap = std::min (spliceTap, limit);
                fadeLength = 0;
                fadePosition = 0;
            }
            else
            {
                //  Le saut a été calé avec le ratio de son début ; si le ratio
                //  monte pendant le fondu, la copie qui s'efface ne doit pas
                //  lire au-delà de l'entrée.
                tap = std::min (tap + ratioNow, limit);
            }
        }
        else
        {
            for (int ch = 0; ch < channels; ++ch)
                wet[ch] = (double) readHist (ch, tap);

            tap = std::min (tap + ratioNow, limit);
        }

        // ---- 7. mélange sec / traité, aligné sur la latence annoncée --------
        const double dry = (double) readHist (0, dryTap);
        dryTap = std::min (dryTap + 1.0, limit);

        for (int ch = 0; ch < channels; ++ch)
        {
            const double d = (ch == 0) ? dry : (double) readHist (ch, dryTap - 1.0);
            out[ch][offset + i] = (float) (d + (wet[ch] - d) * mixNow);
        }
    }
}

} // namespace tune
