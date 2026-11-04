//  dsp_offline.cpp — tests numériques du moteur, SANS DAW, SANS JUCE.
//
//  But : vérifier objectivement la justesse du moteur avant de l'écouter.
//    - précision de la détection (MPM/NSDF)
//    - justesse de la correction (ligne a retard, mode Live)
//    - latence réelle annoncée
//    - transparence quand amount = 0
//    - absence de NaN / instabilité
//
//  Compilation :  g++ -O2 -std=c++17 -I Source/dsp tests/dsp_offline.cpp \
//                     Source/dsp/PitchDetector.cpp Source/dsp/LiveEngine.cpp \
//                     Source/dsp/TuneCore.cpp -o dsp_tests
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <cstring>

#include "PitchDetector.h"
#include "NoteMapper.h"
#include "TuneCore.h"

using namespace tune;

static int gFailures = 0;

static void check (bool ok, const char* label, const char* detail = "")
{
    std::printf ("  [%s] %-52s %s\n", ok ? " OK " : "FAIL", label, detail);
    if (! ok) ++gFailures;
}

// ---------------------------------------------------------------------------
//  Estimation de f0 sur un buffer (autocorrélation + interpolation parabolique)
// ---------------------------------------------------------------------------
static double estimateF0 (const std::vector<float>& x, int start, int len, double sr,
                          double fmin = 60.0, double fmax = 1200.0)
{
    if (start + len > (int) x.size()) len = (int) x.size() - start;
    if (len < 512) return 0.0;

    const int tauMin = (int) std::floor (sr / fmax);
    const int tauMax = (int) std::ceil  (sr / fmin);
    const int integ  = len - tauMax;
    if (integ < 128) return 0.0;

    // retirer la composante continue
    double mean = 0.0;
    for (int i = 0; i < len; ++i) mean += x[start + i];
    mean /= len;

    std::vector<double> r ((size_t) tauMax + 2, 0.0);
    for (int tau = 0; tau <= tauMax; ++tau)
    {
        double s = 0.0;
        for (int j = 0; j < integ; ++j)
        {
            const double a = x[start + j] - mean;
            const double b = x[start + j + tau] - mean;
            s += a * b;
        }
        r[(size_t) tau] = s;
    }

    // premier maximum local après le premier passage par zéro
    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (r[(size_t) tau] > r[(size_t) (tau - 1)] && r[(size_t) tau] >= r[(size_t) (tau + 1)])
        {
            if (r[(size_t) tau] > 0.0) { best = tau; break; }
        }
    }
    if (best < 0) return 0.0;

    // interpolation parabolique
    double dtau = 0.0;
    const double a = r[(size_t) (best - 1)], b = r[(size_t) best], c = r[(size_t) (best + 1)];
    const double denom = a - 2.0 * b + c;
    if (std::abs (denom) > 1e-20) dtau = 0.5 * (a - c) / denom;

    return sr / ((double) best + dtau);
}


// Mesure de fréquence très précise autour d'une fréquence attendue
// (balayage fin + interpolation parabolique). Adaptée aux signaux quasi sinusoïdaux.
static double measureFreqNear (const std::vector<float>& x, int start, int len,
                               double expected, double sr, double spanPct = 0.04)
{
    if (start + len > (int) x.size()) len = (int) x.size() - start;
    const int fLo = -200, fHi = 200;                       // indices de balayage
    const double f0 = expected * (1.0 - spanPct);
    const double df = expected * 2.0 * spanPct / (double) (fHi - fLo);

    std::vector<double> mag ((size_t) (fHi - fLo + 1), 0.0);
    for (int k = fLo; k <= fHi; ++k)
    {
        const double f = f0 + df * (double) (k - fLo);
        const double w = 2.0 * kPi * f / sr;
        // Goertzel
        double s0 = 0.0, s1 = 0.0, s2 = 0.0;
        const double coef = 2.0 * std::cos (w);
        for (int i = 0; i < len; ++i)
        {
            s0 = (double) x[(size_t) (start + i)] + coef * s1 - s2;
            s2 = s1; s1 = s0;
        }
        mag[(size_t) (k - fLo)] = s1 * s1 + s2 * s2 - coef * s1 * s2;
    }

    int best = 0;
    for (int i = 0; i < (int) mag.size(); ++i) if (mag[(size_t) i] > mag[(size_t) best]) best = i;

    double shift = 0.0;
    if (best > 0 && best + 1 < (int) mag.size())
    {
        const double a = mag[(size_t) (best - 1)], b = mag[(size_t) best], c = mag[(size_t) (best + 1)];
        const double den = a - 2.0 * b + c;
        if (std::abs (den) > 1e-20) shift = 0.5 * (a - c) / den;
    }
    return f0 + df * ((double) best + shift);
}

static std::vector<float> makeSine (double hz, double sr, int n, double amp = 0.5)
{
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i)
        v[(size_t) i] = (float) (amp * std::sin (2.0 * kPi * hz * (double) i / sr));
    return v;
}

// Sawtooth "voix" grossière : harmoniques décroissantes -> signal riche en formants
static std::vector<float> makeSaw (double f0, double sr, int n, double amp = 0.4)
{
    std::vector<float> v ((size_t) n, 0.0f);
    for (int h = 1; h <= 24; ++h)
    {
        const double f = f0 * h;
        if (f > sr * 0.45) break;
        const double a = amp / (double) h;
        for (int i = 0; i < n; ++i)
            v[(size_t) i] += (float) (a * std::sin (2.0 * kPi * f * (double) i / sr));
    }
    return v;
}

static bool allFinite (const std::vector<float>& v, double limit = 10.0)
{
    for (float s : v)
        if (! std::isfinite (s) || std::abs (s) > limit) return false;
    return true;
}

// ---------------------------------------------------------------------------
//  TEST 1 — précision de la détection MPM/NSDF
// ---------------------------------------------------------------------------
static void testDetection()
{
    std::printf ("\nTEST 1 — detection MPM/NSDF (70-1000 Hz)\n");
    const double sr = 48000.0;
    const double freqs[] = { 82.4, 110.0, 146.8, 220.0, 329.6, 440.0, 660.0, 880.0 };

    for (double f : freqs)
    {
        auto sig = makeSine (f, sr, 24000);
        PitchDetector det;
        det.setRange (70.0, 1000.0);          // plage live imposee
        det.prepare (sr, 1024, 128);

        // moyenne des estimations valides de la seconde moitie
        double sum = 0.0; int count = 0;
        for (int pos = 0; pos + 128 <= (int) sig.size(); pos += 128)
        {
            const auto ed = det.process (sig.data() + pos, 128);
            if (! ed.valid || ed.hz <= 0.0) continue;
            if (pos < 12000) continue;        // on laisse la fenetre se remplir
            sum += ed.hz; ++count;
        }

        const double hz = count > 0 ? sum / (double) count : 0.0;
        const double cents = (hz > 0.0) ? std::abs (ratioToCents (hz / f)) : 9999.0;

        char detail[128];
        std::snprintf (detail, sizeof (detail), "attendu %6.1f Hz, mesure %6.1f Hz (%.2f cents)",
                       f, hz, cents);
        check (cents < 10.0, "detection sinus", detail);
    }
}

// ---------------------------------------------------------------------------
//  TEST 2 — gammes et note la plus proche
// ---------------------------------------------------------------------------
static void testScaleMapping()
{
    std::printf ("\nTEST 2 — gammes et note cible\n");

    NoteMapper m;
    m.setKey (0);                 // C
    m.setScale (NoteMapper::Major);

    // C4 = 60 : autorisée
    check (m.nearestNote (60.0) == 60, "C majeur : C4 reste C4");
    // C#4 (61) -> doit partir vers C4 ou D4 (le plus proche : C4)
    check (m.nearestNote (61.0) == 60 || m.nearestNote (61.0) == 62, "C majeur : C#4 quantifie");
    // F#4 (66) -> G4 (67) ou F4 (65)
    check (m.nearestNote (66.0) == 65 || m.nearestNote (66.0) == 67, "C majeur : F#4 quantifie");

    // Doit rester dans la gamme
    bool allInScale = true;
    for (int n = 48; n <= 84; ++n)
        if (! m.isPitchClassAllowed (m.nearestNote ((double) n)))
            allInScale = false;
    check (allInScale, "toutes les notes cibles sont dans la gamme");

    // Gamme mineure naturelle en A
    m.setKey (9);
    m.setScale (NoteMapper::MinorNatural);
    check (m.isPitchClassAllowed (57), "A mineur contient A (57)");
    check (m.isPitchClassAllowed (60), "A mineur contient C (60)");
    check (! m.isPitchClassAllowed (61), "A mineur exclut C# (61)");

    // Vérification des masques : 7 notes pour une gamme diatonique
    {
        struct { NoteMapper::Scale s; int expected; const char* name; } cases[] = {
            { NoteMapper::Major,           7, "majeur" },
            { NoteMapper::MinorNatural,    7, "mineur naturel" },
            { NoteMapper::MinorHarmonic,   7, "mineur harmonique" },
            { NoteMapper::PentatonicMajor, 5, "pentatonique majeur" },
            { NoteMapper::PentatonicMinor, 5, "pentatonique mineur" },
            { NoteMapper::Blues,           6, "blues" },
            { NoteMapper::Dorian,          7, "dorien" },
            { NoteMapper::Chromatic,      12, "chromatique" },
        };
        for (auto& c : cases)
        {
            const unsigned m = NoteMapper::scaleMask (c.s);
            int count = 0;
            for (int b = 0; b < 12; ++b) if ((m >> b) & 1u) ++count;
            char label[96];
            std::snprintf (label, sizeof (label), "gamme %s : %d notes", c.name, count);
            check (count == c.expected, label);
        }

        // le mineur harmonique doit contenir la sensible (11) et pas le 6e degre bis
        const unsigned mh = NoteMapper::scaleMask (NoteMapper::MinorHarmonic);
        check (((mh >> 11) & 1u) != 0, "mineur harmonique contient la sensible (11)");
        check (((mh >> 6) & 1u) == 0, "mineur harmonique exclut le triton (6)");
    }

    // Chromatique : identité
    m.setScale (NoteMapper::Chromatic);
    bool identity = true;
    for (int n = 40; n <= 80; ++n)
        if (m.nearestNote ((double) n) != n) identity = false;
    check (identity, "chromatique = identite");
}

// ---------------------------------------------------------------------------
//  TEST 3 — correction de hauteur (le cœur)
// ---------------------------------------------------------------------------
struct RunResult
{
    std::vector<float> out;
    int latency = 0;
};

static RunResult runCore (const std::vector<float>& in, double sr,
                          TuneCore::Params p, int blockSize,
                          NoteMapper::Scale scale = NoteMapper::Chromatic,
                          int key = 0)
{
    TuneCore core;
    core.prepare (sr, blockSize, 1);
    core.setParams (p);
    core.notes().setKey (key);
    core.notes().setScale (scale);

    RunResult r;
    r.latency = core.latencySamples();
    r.out.assign (in.size(), 0.0f);

    int pos = 0;
    while (pos < (int) in.size())
    {
        const int n = std::min (blockSize, (int) in.size() - pos);
        const float* ip[1] = { in.data() + pos };
        float*       op[1] = { r.out.data() + pos };
        core.process (ip, op, n, 1);
        pos += n;
    }
    return r;
}

static void testPitchCorrection()
{
    std::printf ("\nTEST 3 \u2014 correction de hauteur\n");
    const double sr = 48000.0;
    const int    N  = 96000;              // 2 s

    // --- 3a : sinus desaccordé, correction sur note chromatique -----------
    {
        const double fIn = 227.0;         // ~A#4 - 46 cents  (note cible : 233.08 Hz)
        const double fTarget = midiToHz (std::lround (hzToMidi (fIn)));

        auto in = makeSine (fIn, sr, N);
        TuneCore::Params p;
        p.amount = 1.0f; p.retuneMs = 0.0f; p.mix = 1.0f; p.transpose = 0;

        auto r = runCore (in, sr, p, 512);
        const double fOut = measureFreqNear (r.out, N / 2, 20000, fTarget, sr);

        const double errCents = std::abs (ratioToCents (fOut / fTarget));
        char detail[160];
        std::snprintf (detail, sizeof (detail),
                       "entree %.1f Hz -> sortie %.1f Hz (cible %.1f Hz, ecart %.1f cents)",
                       fIn, fOut, fTarget, errCents);
        check (errCents < 15.0, "correction d'un sinus desaccorde", detail);
    }

    // --- 3b : sawtooth (plus proche d'une voix) ---------------------------
    {
        const double fIn = 148.0;         // ~D3 + 35 cents ; cible : D3 = 146.83
        const double fTarget = midiToHz (std::lround (hzToMidi (fIn)));

        auto in = makeSaw (fIn, sr, N);
        TuneCore::Params p;
        p.amount = 1.0f; p.retuneMs = 0.0f; p.formant = 0.0f;

        auto r = runCore (in, sr, p, 512);
        const double fOut = estimateF0 (r.out, N / 2, 24000, sr);
        const double errCents = std::abs (ratioToCents (fOut / fTarget));

        char detail[160];
        std::snprintf (detail, sizeof (detail),
                       "entree %.1f Hz -> sortie %.1f Hz (cible %.1f Hz, ecart %.1f cents)",
                       fIn, fOut, fTarget, errCents);
        check (errCents < 20.0, "correction d'une sawtooth", detail);
        check (allFinite (r.out), "aucune valeur invalide en sortie");
    }

    // --- 3c : correction vers une note plus grave -------------------------
    {
        const double fIn = 225.0;         // ~A3 + 39 cents ; cible A3 = 220
        const double fTarget = midiToHz (std::lround (hzToMidi (fIn)));

        auto in = makeSine (fIn, sr, N);
        TuneCore::Params p;
        p.amount = 1.0f; p.retuneMs = 0.0f;

        auto r = runCore (in, sr, p, 512);
        const double fOut = measureFreqNear (r.out, N / 2, 20000, fTarget, sr);
        const double errCents = std::abs (ratioToCents (fOut / fTarget));

        char detail[160];
        std::snprintf (detail, sizeof (detail),
                       "entree %.1f Hz -> sortie %.1f Hz (cible %.1f Hz, ecart %.1f cents)",
                       fIn, fOut, fTarget, errCents);
        check (errCents < 20.0, "correction vers le grave", detail);
    }
}

// ---------------------------------------------------------------------------
//  TEST 4 — latence réelle
// ---------------------------------------------------------------------------
static void testLatency()
{
    std::printf ("\nTEST 4 \u2014 latence\n");
    const double sr = 48000.0;
    const int N = 96000;

    // Signal sinusoidal, correction desactivee : la sortie doit etre l'entree
    // retardee de la latence annoncee (le moteur a fenetre etale les impulsions,
    // donc on mesure par correlation et non par pic).
    std::mt19937 rng (12345u);
    std::uniform_real_distribution<float> dist (-0.3f, 0.3f);
    std::vector<float> in ((size_t) N);
    for (auto& v : in) v = dist (rng);

    TuneCore::Params p; p.amount = 0.0f; p.retuneMs = 0.0f; p.formant = 0.0f;

    auto r = runCore (in, sr, p, 512);

    double best = -2.0; int bestLag = 0;
    for (int lag = r.latency - 400; lag <= r.latency + 400; ++lag)
    {
        if (lag < 0) continue;
        double num = 0.0, d1 = 0.0, d2 = 0.0;
        for (int i = 20000; i < 60000; ++i)
        {
            const double a = in[(size_t) (i - lag)];
            const double b = r.out[(size_t) i];
            num += a * b; d1 += a * a; d2 += b * b;
        }
        const double c = num / std::sqrt (d1 * d2 + 1e-12);
        if (c > best) { best = c; bestLag = lag; }
    }

    char detail[160];
    std::snprintf (detail, sizeof (detail),
                   "annoncee %d samples (%.1f ms), mesuree %d samples",
                   r.latency, 1000.0 * r.latency / sr, bestLag);

    check (std::abs (bestLag - r.latency) <= 120,
           "latence reelle = latence annoncee (ecart <= 2.5 ms)", detail);
    //  v0.8.0 : le moteur PSOLA (26 ms) est remplace par une ligne a retard a
    //  lecture pitch-synchrone. La latence ne depend plus de la note ni de la
    //  taille d'un anneau : elle vaut 1/400 s plus une constante de quelques
    //  echantillons, soit 2,7 ms a 48 kHz.
    check (r.latency <= (int) (0.005 * sr), "latence <= 5 ms", detail);
}

// ---------------------------------------------------------------------------
//  TEST 5 — transparence quand amount = 0
// ---------------------------------------------------------------------------
static void testTransparency()
{
    std::printf ("\nTEST 5 — transparence (amount = 0)\n");
    const double sr = 48000.0;
    const int N = 96000;

    auto in = makeSaw (196.0, sr, N);

    TuneCore::Params p;
    p.amount = 0.0f; p.retuneMs = 0.0f; p.mix = 1.0f; p.formant = 0.0f;

    auto r = runCore (in, sr, p, 512);

    // Le PSOLA introduit un décalage borné à une demi-période : on cherche le
    // meilleur alignement autour de la latence annoncée.
    auto corrAt = [&] (int lag)
    {
        double num = 0.0, d1 = 0.0, d2 = 0.0;
        for (int i = 20000; i < 60000; ++i)
        {
            const double a = in[(size_t) (i - lag)];
            const double b = r.out[(size_t) i];
            num += a * b; d1 += a * a; d2 += b * b;
        }
        return num / std::sqrt (std::max (1e-20, d1 * d2));
    };

    double bestCorr = -2.0; int bestLag = 0;
    for (int lag = r.latency - 100; lag <= r.latency + 100; ++lag)
    {
        const double c = corrAt (lag);
        if (c > bestCorr) { bestCorr = c; bestLag = lag; }
    }

    char detail[160];
    std::snprintf (detail, sizeof (detail),
                   "correlation %.4f au retard %d (annonce %d)", bestCorr, bestLag, r.latency);
    check (bestCorr > 0.97, "signal reconstruit a l'identique quand amount = 0", detail);
    check (std::abs (bestLag - r.latency) <= 105, "retard reel conforme a la latence annoncee", detail);
}

// ---------------------------------------------------------------------------
//  TEST 6 — silence, bruit, cas dégénérés
// ---------------------------------------------------------------------------
static void testRobustness()
{
    std::printf ("\nTEST 6 — robustesse\n");
    const double sr = 48000.0;
    const int N = 72000;

    // silence
    {
        std::vector<float> in ((size_t) N, 0.0f);
        auto r = runCore (in, sr, TuneCore::Params{}, 512);
        double peak = 0.0;
        for (float s : r.out) peak = std::max (peak, (double) std::abs (s));
        check (peak < 1e-6, "silence -> silence", "");
    }

    // bruit blanc (non voisé)
    {
        std::mt19937 rng (1234);
        std::uniform_real_distribution<float> dist (-0.3f, 0.3f);
        std::vector<float> in ((size_t) N);
        for (auto& s : in) s = dist (rng);

        TuneCore::Params p; p.amount = 1.0f;
        auto r = runCore (in, sr, p, 512);

        check (allFinite (r.out), "bruit -> sortie numeriquement stable", "");

        double peak = 0.0;
        for (float s : r.out) peak = std::max (peak, (double) std::abs (s));
        char detail[96];
        std::snprintf (detail, sizeof (detail), "crete = %.3f", peak);
        check (peak < 1.0, "bruit -> pas d'emballement", detail);
    }

    // blocs de tailles différentes (indépendance vis-à-vis du buffer)
    {
        auto in = makeSaw (180.0, sr, N);
        TuneCore::Params p; p.amount = 1.0f;

        auto rA = runCore (in, sr, p, 64);
        auto rB = runCore (in, sr, p, 2048);

        const double fA = estimateF0 (rA.out, N / 2, 24000, sr);
        const double fB = estimateF0 (rB.out, N / 2, 24000, sr);

        char detail[128];
        std::snprintf (detail, sizeof (detail), "bloc 64 -> %.2f Hz, bloc 2048 -> %.2f Hz", fA, fB);
        check (std::abs (ratioToCents (fA / fB)) < 10.0, "resultat independant de la taille de bloc", detail);
    }
}

// ---------------------------------------------------------------------------
//  TEST 7 — retune speed et amount
// ---------------------------------------------------------------------------
static void testRetuneAndAmount()
{
    std::printf ("\nTEST 7 — retune speed et amount\n");
    const double sr = 48000.0;
    const int N = 96000;
    const double fIn = 227.0;

    // amount = 0.5 -> on doit corriger environ la moitie du chemin
    {
        auto in = makeSine (fIn, sr, N);
        TuneCore::Params p; p.amount = 0.5f; p.retuneMs = 0.0f;
        auto r = runCore (in, sr, p, 512);

        const double fOut = measureFreqNear (r.out, N / 2, 24000, 230.0, sr);
        const double midiIn = hzToMidi (fIn);
        const double target = midiToHz (std::lround (midiIn));
        const double expectedHalf = midiToHz ((midiIn + std::lround (midiIn)) * 0.5);

        char detail[192];
        std::snprintf (detail, sizeof (detail),
                       "entree %.1f -> %.1f Hz (cible 50%% = %.1f Hz, cible 100%% = %.1f Hz)",
                       fIn, fOut, expectedHalf, target);
        const double err = std::abs (ratioToCents (fOut / expectedHalf));
        check (err < 25.0, "amount 50 % = correction a mi-chemin", detail);
    }

    // retune lent : la correction doit etre retardee
    {
        auto in = makeSine (fIn, sr, N);
        TuneCore::Params pFast; pFast.amount = 1.0f; pFast.retuneMs = 0.0f;
        TuneCore::Params pSlow; pSlow.amount = 1.0f; pSlow.retuneMs = 1000.0f;

        auto rFast = runCore (in, sr, pFast, 512);
        auto rSlow = runCore (in, sr, pSlow, 512);

        const int start = (int) (sr * 0.60);
        // gamme chromatique par defaut dans runCore : cible = A#4 = 233.08 Hz
        const double fFast = measureFreqNear (rFast.out, start, 12000, 232.0, sr);
        const double fSlow = measureFreqNear (rSlow.out, start, 12000, 232.0, sr);

        char detail[160];
        std::snprintf (detail, sizeof (detail), "retune 0 ms -> %.1f Hz, retune 1000 ms -> %.1f Hz",
                       fFast, fSlow);
        check (std::abs (ratioToCents (fFast / fSlow)) > 10.0,
               "retune speed change reellement la reponse", detail);
    }
}


// ---------------------------------------------------------------------------
//  TEST 8 — le mono reste mono (pas d'elargissement stereo)
// ---------------------------------------------------------------------------
static void testMonoStaysMono()
{
    std::printf ("\nTEST 8 \u2014 le mono reste mono\n");
    const double sr = 48000.0;
    const int    N  = 48000;

    // Signal identique sur les deux canaux (ce que fait un DAW avec une piste mono)
    auto in = makeSaw (196.0, sr, N);

    TuneCore::Params p; p.amount = 1.0f; p.retuneMs = 0.0f;

    TuneCore core;
    core.prepare (sr, 512, 2);
    core.setParams (p);
    core.notes().setScale (NoteMapper::Chromatic);

    std::vector<float> l ((size_t) N), r ((size_t) N);

    for (int pos = 0; pos < N; pos += 512)
    {
        const int n = std::min (512, N - pos);
        const float* ip[2] = { in.data() + pos, in.data() + pos };
        float*       op[2] = { l.data() + pos,  r.data() + pos };
        core.process (ip, op, n, 2);
    }

    double maxDiff = 0.0, peak = 0.0;
    for (int i = 0; i < N; ++i)
    {
        maxDiff = std::max (maxDiff, (double) std::abs (l[(size_t) i] - r[(size_t) i]));
        peak    = std::max (peak,    (double) std::abs (l[(size_t) i]));
    }

    char detail[160];
    std::snprintf (detail, sizeof (detail),
                   "ecart maxi gauche/droite = %.3e (crete %.3f)", maxDiff, peak);

    check (maxDiff < 1e-6, "entree mono -> sortie mono (L et R identiques)", detail);

    // Le fait de traiter 2 canaux ne doit pas changer le resultat par rapport
    // au traitement d'un seul canal.
    auto mono = runCore (in, sr, p, 512, NoteMapper::Chromatic);
    double maxDiff2 = 0.0;
    for (int i = 0; i < N; ++i)
        maxDiff2 = std::max (maxDiff2, (double) std::abs (l[(size_t) i] - mono.out[(size_t) i]));

    std::snprintf (detail, sizeof (detail),
                   "ecart maxi mono / stereo-mono = %.3e", maxDiff2);
    check (maxDiff2 < 1e-6, "traitement mono = traitement stereo d'un signal mono", detail);
}


// ---------------------------------------------------------------------------
//  TEST 9 — humanize et vibrato (les 2 reglages "humains" de l'interface)
// ---------------------------------------------------------------------------
static double freqCents (const std::vector<float>& x, int start, int len,
                         double sr, double expectHz)
{
    if (start < 0 || start + len > (int) x.size()) return -1.0;
    return measureFreqNear (x, start, len, expectHz, sr);
}

static void testHumanizeVibrato()
{
    std::printf ("\nTEST 9 \u2014 humanize et vibrato\n");
    const double sr = 48000.0;
    const int    N  = 96000;
    const auto   in = makeSine (220.0, sr, N, 0.35);   // A3 : cible 220 Hz exactement

    TuneCore::Params base;
    base.amount = 1.0f;
    base.retuneMs = 0.0f;

    auto spread = [&] (TuneCore::Params p)
    {
        const auto r = runCore (in, sr, p, 512, NoteMapper::Chromatic);
        double lo = 1e9, hi = -1e9;
        for (int k = 0; k < 10; ++k)
        {
            const int start = 24000 + k * 4800;      // 0.5 s -> 1.4 s
            const double f = freqCents (r.out, start, 6000, sr, 220.0);
            if (f <= 0.0) continue;
            const double cents = 1200.0 * std::log2 (f / 220.0);
            lo = std::min (lo, cents);
            hi = std::max (hi, cents);
        }
        return hi - lo;
    };

    const double spreadNone  = spread (base);
    TuneCore::Params ph = base; ph.humanize = 1.0f;
    const double spreadHum   = spread (ph);
    TuneCore::Params pv = base; pv.vibrato = 1.0f; pv.vibratoHz = 6.0f;
    const double spreadVib   = spread (pv);

    char detail[200];
    std::snprintf (detail, sizeof (detail),
                   "ecart de hauteur sur 1 s : sans = %.2f cents, humanize = %.2f, vibrato = %.2f",
                   spreadNone, spreadHum, spreadVib);
    check (spreadHum > 1.5 && spreadHum > spreadNone * 2.0,
           "humanize fait deriver la hauteur", detail);
    check (spreadVib > 20.0, "vibrato fait osciller la hauteur", detail);

    // humanize a 0 et vibrato a 0 -> aucun mouvement (le hard tune reste droit)
    check (spreadNone < 1.0, "sans humanize ni vibrato, la hauteur ne bouge pas", detail);
}

// ---------------------------------------------------------------------------
//  TEST 10 — vu-metres entree / sortie
// ---------------------------------------------------------------------------
static void testMeters()
{
    std::printf ("\nTEST 10 \u2014 vu-metres entree / sortie\n");
    const double sr = 48000.0;
    const int    N  = 48000;

    auto runMeter = [&] (double amp)
    {
        const auto in = makeSine (220.0, sr, N, amp);

        TuneCore core;
        core.prepare (sr, 512, 1);
        TuneCore::Params p;
        p.amount = 1.0f;
        core.setParams (p);
        core.notes().setScale (NoteMapper::Chromatic);

        std::vector<float> out (in.size(), 0.0f);
        int pos = 0;
        while (pos < (int) in.size())
        {
            const int n = std::min (512, (int) in.size() - pos);
            const float* ip[1] = { in.data() + pos };
            float*       op[1] = { out.data() + pos };
            core.process (ip, op, n, 1);
            pos += n;
        }
        return std::pair<float,float> (core.getInputPeakDb(), core.getOutputPeakDb());
    };

    const auto m = runMeter (0.5);       // crete 0.5 -> -6 dB
    char detail[200];
    std::snprintf (detail, sizeof (detail),
                   "entree 0.5 -> peak %.1f dB ; sortie -> peak %.1f dB (attendu ~ -6)",
                   m.first, m.second);
    check (std::abs (m.first + 6.0f) < 1.5f, "vu-metre ENTREE mesure la bonne crete", detail);
    check (m.second > -12.0f, "vu-metre SORTIE recoit du signal", detail);

    const auto s = runMeter (0.0);
    std::snprintf (detail, sizeof (detail), "silence -> entree %.1f dB", s.first);
    check (s.first <= -90.0f, "vu-metre au silence = -inf", detail);
}

// ---------------------------------------------------------------------------
//  TEST 11 — les sibilantes passent en direct (anti-broutage)
// ---------------------------------------------------------------------------
static void testSibilantBypass()
{
    std::printf ("\nTEST 11 \u2014 sibilantes en direct (anti-broutage)\n");
    const double sr = 48000.0;
    const int    N  = 96000;                       // 2 s
    const int    sibStart = (int) (0.80 * sr);
    const int    sibEnd   = (int) (1.20 * sr);

    auto in = makeSaw (152.0, sr, N);              // +40 cents : correction nette

    //  « S » synthétique : bruit blanc différentié (énergie dans l'aigu), sans
    //  aucune périodicité. C'est exactement ce que le détecteur doit refuser.
    {
        std::mt19937 rng (777u);
        std::normal_distribution<float> nd (0.0f, 0.12f);
        std::vector<float> hiss ((size_t) N, 0.0f);
        float prev = 0.0f;
        for (int i = sibStart; i < sibEnd; ++i)
        {
            const float n = nd (rng);
            hiss[(size_t) i] = n - prev;
            prev = n;
        }
        for (int i = sibStart; i < sibEnd; ++i)
            in[(size_t) i] = hiss[(size_t) i];
    }

    TuneCore::Params p;
    p.amount = 1.0f; p.retuneMs = 0.0f; p.formant = 0.0f;
    auto r = runCore (in, sr, p, 512);

    // 1. la sibilante doit ressortir TELLE QUELLE (simple retard)
    {
        const int a = sibStart + 4000;
        const int b = sibEnd   - 4000;
        double num = 0.0, d1 = 0.0, d2 = 0.0;
        for (int i = a; i < b; ++i)
        {
            const double x = in[(size_t) (i - r.latency)];
            const double y = r.out[(size_t) i];
            num += x * y; d1 += x * x; d2 += y * y;
        }
        const double c = num / std::sqrt (std::max (1e-20, d1 * d2));
        char detail[160];
        std::snprintf (detail, sizeof (detail),
                       "correlation entree/sortie sur le \"s\" = %.5f (1.00000 = passe-through)", c);
        check (c > 0.999, "le \"s\" passe en direct, sans traitement", detail);
    }

    // 2. la partie voisée reste corrigée (152 Hz -> 155.56 Hz)
    {
        const double fOut = measureFreqNear (r.out, (int) (0.35 * sr), 12000, 155.56, sr);
        const double err  = std::abs (ratioToCents (fOut / 155.56));
        char detail[160];
        std::snprintf (detail, sizeof (detail), "152 Hz -> %.1f Hz (cible 155.6, ecart %.1f cents)",
                       fOut, err);
        check (err < 25.0, "la partie voisee est bien corrigee", detail);
    }
}

// ---------------------------------------------------------------------------
//  TEST 12 — préservation des formants (pas d'effet « Mickey Mouse »)
// ---------------------------------------------------------------------------
static double goertzelMag (const std::vector<float>& x, int start, int len,
                           double f, double sr)
{
    if (start < 0 || start + len > (int) x.size() || len < 16) return 0.0;
    const double w = 2.0 * kPi * f / sr;
    const double coef = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < len; ++i)
    {
        const double s0 = (double) x[(size_t) (start + i)] + coef * s1 - s2;
        s2 = s1; s1 = s0;
    }
    const double m = s1 * s1 + s2 * s2 - coef * s1 * s2;
    return std::sqrt (std::max (0.0, m));
}

//  Pic spectral par balayage fin : plus fiable que l'autocorrelation globale sur
//  un signal PSOLA, ou la repetition des grains cree des pics secondaires.
static double spectralPeak (const std::vector<float>& x, int start, int len,
                            double lo, double hi, double sr)
{
    double bestF = 0.0, bestV = -1.0, prev = 0.0, cur = 0.0;
    double a = 0.0, b = 0.0, c = 0.0;

    for (double f = lo; f <= hi; f += 2.0)
    {
        const double m = goertzelMag (x, start, len, f, sr);
        if (m > bestV)
        {
            bestV = m; bestF = f; a = prev; b = m;
        }
        prev = m;
        cur = f;
    }
    (void) cur;

    // affinage : on refait localement autour du pic
    double fine = bestF, fineBest = -1.0;
    a = b; c = 0.0;
    for (double f = bestF - 2.5; f <= bestF + 2.5; f += 0.5)
    {
        const double m = goertzelMag (x, start, len, f, sr);
        if (m > fineBest) { fineBest = m; fine = f; }
    }
    (void) a; (void) b; (void) c;
    return fine;
}

// ---------------------------------------------------------------------------
//  TEST 12 — les sauts de cycle ne cliquent pas
// ---------------------------------------------------------------------------
//  Pour changer la hauteur, le moteur saute une periode en avant ou en arriere
//  des que le point de lecture a derive d'un cycle. Sur un signal periodique ce
//  saut est exactement continu ; sur un signal qui bouge (voyelle, note qui
//  demarre) il ne l'est pas, et c'est le fondu qui doit absorber l'ecart. Ce
//  test verifie qu'aucune discontinuite ne sort du moteur : sur une sinusoide,
//  la plus grande difference entre deux echantillons consecutifs de la sortie ne
//  doit pas depasser celle de la sinusoide elle-meme.
static void testSplicesWithoutClicks()
{
    std::printf ("\nTEST 12 \u2014 sauts de cycle sans clic\n");
    const double sr = 48000.0;
    const int    N  = 96000;
    const double f0 = 220.0;

    std::vector<float> in ((size_t) N, 0.0f);
    for (int i = 0; i < N; ++i)
        in[(size_t) i] = (float) (0.4 * std::sin (2.0 * kPi * f0 * (double) i / sr));

    //  Transposition de +1 demi-ton : le rapport de lecture vaut 1,0595 a
    //  chaque echantillon, donc le point de lecture derive en permanence et le
    //  moteur saute un cycle toutes les ~17 ms.
    TuneCore::Params p;
    p.amount = 1.0f; p.retuneMs = 0.0f; p.noteRampMs = 0.0f;
    p.transpose = 1;
    auto r = runCore (in, sr, p, 512);

    const double fOut = f0 * std::pow (2.0, 1.0 / 12.0);

    //  Pente maximale theorique d'une sinusoide d'amplitude 0,4 a fOut.
    const double slopeTheo = 0.4 * 2.0 * kPi * fOut / sr;

    double maxStep = 0.0;
    int    jumps   = 0;
    const int a = 12000, b = N - 12000;

    for (int i = a + 1; i < b; ++i)
    {
        const double d = std::abs ((double) r.out[(size_t) i] - (double) r.out[(size_t) (i - 1)]);
        maxStep = std::max (maxStep, d);
        if (d > 2.5 * slopeTheo) ++jumps;
    }

    char detail[220];
    std::snprintf (detail, sizeof (detail),
                   "pente max %.5f (theorique %.5f, rapport %.2f) ; %d sauts > 2,5 x la pente",
                   maxStep, slopeTheo, maxStep / std::max (1e-12, slopeTheo), jumps);
    check (maxStep < 2.5 * slopeTheo,
           "aucune discontinuite aux sauts de cycle", detail);

    //  et la hauteur est bien celle demandee
    const double measured = measureFreqNear (r.out, N / 2 - 6000, 12000, fOut, sr);
    const double err = std::abs (ratioToCents (measured / fOut));
    std::snprintf (detail, sizeof (detail), "%.2f Hz mesures, cible %.2f Hz (ecart %.1f cents)",
                   measured, fOut, err);
    check (err < 20.0, "le saut de cycle corrige bien la hauteur", detail);
}

// ---------------------------------------------------------------------------
//  TEST 13 — la correction est appliquee sans delai
// ---------------------------------------------------------------------------
//  C'est le test de regression du defaut de la v0.7.0 : le moteur PSOLA
//  ecrivait la moitie de chaque grain derriere la tete de lecture, si bien que
//  la sortie restait a l'ancienne hauteur pendant 72 ms avant de sauter d'un
//  demi-ton d'un coup (mesure : degressive /tmp/eng4.cpp). Ici la reponse doit
//  arriver en moins de 35 ms, ce qui inclut la rampe de note de 21 ms.
static void testImmediateResponse()
{
    std::printf ("\nTEST 13 \u2014 reponse immediate du moteur\n");
    const double sr = 48000.0;
    const int    N  = 96000;
    const int    step = N / 2;

    std::vector<float> in ((size_t) N, 0.0f);
    for (int i = 0; i < N; ++i)
    {
        const double f = (i < step) ? 220.0 : 233.082;      // A3 -> A#3 (un demi-ton)
        in[(size_t) i] = (float) (0.4 * std::sin (2.0 * kPi * f * (double) i / sr));
    }

    TuneCore::Params p;
    p.amount = 1.0f; p.retuneMs = 0.0f;
    auto r = runCore (in, sr, p, 512);

    //  Mesure par passages par zero interpoles : la periode entre deux passages
    //  croissants donne la frequence INSTANTANEE, avec une resolution d'un
    //  echantillon. Une fenetre de Goertzel ne suffirait pas : 512 echantillons
    //  a 48 kHz font 94 Hz de resolution, et 220 / 233 Hz tombent dans le meme
    //  lobe (verifie : le test ne distinguait rien).
    std::vector<double> cross;   // instants des passages par zero croissants

    for (int i = 1; i < N; ++i)
    {
        const double a = r.out[(size_t) (i - 1)], b = r.out[(size_t) i];
        if (a <= 0.0 && b > 0.0)
            cross.push_back ((double) (i - 1) + a / (a - b));
    }

    int switchAt = -1;
    double lastF[3] = { 0.0, 0.0, 0.0 };

    for (size_t k = 1; k < cross.size(); ++k)
    {
        const double dt = cross[k] - cross[k - 1];
        if (dt < 40.0 || dt > 400.0) continue;          // saute les passages parasites

        const double f = sr / dt;

        lastF[0] = lastF[1]; lastF[1] = lastF[2]; lastF[2] = f;

        //  mediane des trois dernieres periodes : un saut de cycle isole ne
        //  doit pas faire croire a un changement de hauteur
        double a = lastF[0], b = lastF[1], c = lastF[2];
        const double med = std::max (std::min (a, b), std::min (std::max (a, b), c));

        if (cross[k] > (double) step && med > 226.0) { switchAt = (int) cross[k]; break; }
    }

    char detail[200];

    if (switchAt < 0)
    {
        check (false, "la sortie change de hauteur apres le changement de note", "");
        return;
    }

    const double ms = 1000.0 * (double) (switchAt - step) / sr;
    std::snprintf (detail, sizeof (detail),
                   "changement de hauteur effectif %.1f ms apres l'entree (v0.7.0 : 72 ms)", ms);
    check (ms < 35.0, "reponse du moteur < 35 ms", detail);
}

// ---------------------------------------------------------------------------
int main()
{
    std::printf ("=== Tests hors DAW du moteur de correction ===\n");

    testDetection();
    testScaleMapping();
    testPitchCorrection();
    testLatency();
    testTransparency();
    testRobustness();
    testRetuneAndAmount();
    testMonoStaysMono();
    testHumanizeVibrato();
    testMeters();
    testSibilantBypass();
    testSplicesWithoutClicks();
    testImmediateResponse();

    std::printf ("\n=== %s (%d echec(s)) ===\n",
                 gFailures == 0 ? "TOUS LES TESTS PASSENT" : "ECHECS", gFailures);
    return gFailures == 0 ? 0 : 1;
}
