//  TuneCore.cpp — v0.7.0 (MPM + TD-PSOLA, plus de Rubber Band).
#include "TuneCore.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace tune
{

namespace
{
    //  Tranche de traitement FIXE. Le détecteur analyse tous les 128
    //  échantillons et le moteur travaille par tranches de 64 : la décision de
    //  hauteur tombe donc toujours aux mêmes instants, quelle que soit la taille
    //  de bloc du DAW. C'est ce qui garantit un résultat identique en 64 et en
    //  2048 échantillons (test 6).
    constexpr int    kChunk = 64;
    constexpr int    kAnalysisHop = 128;

    //  Seuil de bruit : sous -55 dBFS, on considère qu'il n'y a rien à accorder.
    //  Évite d'accorder le souffle de la salle sur scène.
    constexpr double kGateDb = -55.0;

    //  Clarté minimale du détecteur pour décider « voisé ».
    constexpr double kVoicedClarity = 0.45;

    //  Demande maximale au moteur (±600 cents). Au-delà, on est sur une erreur
    //  de détection, pas sur un chanteur faux.
    constexpr double kMaxDemandCents = 600.0;


    //  Hystérésis de la note visée (en demi-tons) : évite que la voix bascule
    //  d'une note à l'autre sur un vibrato large.
    constexpr double kStickyDrop = 0.80;
    constexpr double kStickyBias = 0.08;

    //  GARDE DE CONTINUITÉ. Un saut de plus de 120 cents d'une analyse à la
    //  suivante (2,7 ms) n'est pas un chanteur : c'est le détecteur qui hésite
    //  entre deux décalages (typiquement sur une voyelle très résonante ou
    //  près d'une consonne). On exige donc DEUX analyses concordantes avant
    //  d'accepter un tel saut. Coût : 5 ms de retard sur un vrai changement de
    //  note ; bénéfice : plus aucune bascule de note parasite en live.
    constexpr double kJumpCents     = 120.0;
    constexpr double kConfirmCents  = 60.0;
}

// ============================================================================
double TuneCore::linToDb (double x) noexcept
{
    return 20.0 * std::log10 (std::max (1.0e-5, std::abs (x)));
}

// dérive aléatoire lente, bornée à -1..+1 (marche filtrée)
double TuneCore::humanizeCents()
{
    if (--humanizeTicks <= 0)
    {
        humanizeTicks = std::max (1, (int) std::lround (0.200 * sr / (double) kAnalysisHop));
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
    ready = false;

    sr          = (sampleRate > 0.0 ? sampleRate : 48000.0);
    maxBlock    = std::max (32, maximumBlockSize);
    numChannels = std::max (1, std::min (2, channels));

    //  Plage imposée en live : 70 Hz (basse) à 1000 Hz (soprano). En dessous de
    //  70 Hz on ne trouve que des bruits de manipulation, au-dessus de 1000 Hz
    //  des harmoniques — les deux produisaient de fausses notes sur scène.
    detector.setRange (70.0, 1000.0);
    detector.prepare (sr, 1024, kAnalysisHop);

    engine.prepare (sr, maxBlock, numChannels);

    //  PLUS DE RETARD DE COMMANDE (v0.8.0).
    //
    //  La v0.7.0 devait différer la commande de 448 échantillons (10 ms) pour
    //  compenser son propre moteur : la latence de 26 ms dépassait la
    //  demi-fenêtre d'analyse de 29,4 ms, si bien que la correction appliquée
    //  décrivait un son plus ancien que celui qu'on fabriquait.
    //
    //  Ici la fenêtre d'analyse est dimensionnée sur la période suivie (≈ 2,2
    //  périodes, soit 5 à 20 ms). Sa demi-fenêtre est donc du même ordre que la
    //  latence du moteur (2,7 ms) : l'écart restant vaut quelques millisecondes
    //  et va dans le bon sens (l'analyse décrit un son plus ancien que le point
    //  de lecture, donc la commande arrive après coup). Il n'y a plus rien à
    //  différer, et l'anneau de commande disparaît avec.
    detector.setAnalysisWindow (0);     // fenêtre maximale tant qu'on n'entend rien

    reset();
    ready = true;
}

void TuneCore::reset()
{
    detector.reset();
    engine.reset();

    voicedNow = false;

    stickyValid = false;
    stickyNote  = 60;
    lastTarget  = -1;
    lastHz      = 0.0;
    candHz      = 0.0;
    candCount   = 0;
    ratioTarget = 1.0;
    ratioSm     = 1.0;
    logHzSm     = std::log (220.0);
    periodSm    = sr / 220.0;
    mixTarget   = 0.0;
    holdSamples = 0;

    humanizeTicks  = 1;
    humanizeWalk   = 0.0;
    humanizeTarget = 0.0;
    vibratoPhase   = 0.0;
    rngState       = 0x12345678u;

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
//  Décision de hauteur
// ============================================================================
void TuneCore::updateDecision (const PitchDetector::Estimate& e)
{
    if (! e.valid)
        return;

    const int    hop     = std::max (1, detector.getHopSize());
    const double hopSec  = (double) hop / sr;

    const double levelDb = linToDb (std::max (e.rms, 1.0e-7));
    const bool   aboveGate = levelDb > kGateDb;

    bool voiced = e.voiced && aboveGate && e.confidence >= kVoicedClarity;

    //  MAINTIEN APRÈS CONSONNE : une plosive coupe la périodicité pendant 10 à
    //  30 ms. Sans maintien, la correction se couperait au milieu de chaque
    //  syllabe (« voix qui hoquette »). On garde donc la correction pendant
    //  `sibilantMs` après la dernière trame voisée.
    if (voiced)
    {
        holdSamples = (int) std::lround ((double) params.sibilantMs * 0.001 * sr);
    }
    else if (holdSamples > 0)
    {
        holdSamples -= hop;
        if (holdSamples > 0)
            voiced = true;
    }

    if (voiced && e.hz > 0.0)
    {
        // ---- garde de continuité --------------------------------------------
        double hz = e.hz;

        if (lastHz > 0.0)
        {
            const double jump = std::abs (ratioToCents (hz / lastHz));

            if (jump > kJumpCents)
            {
                if (candCount > 0 && std::abs (ratioToCents (hz / candHz)) < kConfirmCents)
                    ++candCount;
                else
                {
                    candHz    = hz;
                    candCount = 1;
                }

                if (candCount < 2)
                    hz = lastHz;              // on garde la note tenue
                else
                {
                    lastHz    = hz;
                    candCount = 0;
                }
            }
            else
            {
                lastHz    = hz;
                candCount = 0;
            }
        }
        else
        {
            lastHz = hz;
        }

        // ---- note visée ------------------------------------------------------
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

            if (dSticky > kStickyDrop || dCand + kStickyBias < dSticky)
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
            vibratoPhase += 2.0 * 3.14159265358979 * hz * (double) hop / sr;
            if (vibratoPhase > 6.283185307179586)
                vibratoPhase -= 6.283185307179586;
            cents += std::sin (vibratoPhase) * (double) params.vibrato * 50.0;
        }

        cents = std::min (kMaxDemandCents, std::max (-kMaxDemandCents, cents));

        // ---- lissage exponentiel (LERP) --------------------------------------
        //  On lisse la HAUTEUR DE SORTIE (en logarithme, donc en cents), et non
        //  le rapport de transposition. C'est une différence de fond :
        //  si on lisse le rapport, une petite erreur sur la période de référence
        //  se transforme directement en erreur de justesse sur la sortie
        //  (mesuré : 11 cents sur voix réelle). En lissant la hauteur visée, la
        //  sortie vaut exactement cette hauteur, quelle que soit l'estimation de
        //  période, et le rapport est recalculé à chaque tranche par
        //  r = hauteurVisée x période / sr.
        //
        //  Deux constantes de temps :
        //   - 6 ms mini pour suivre une dérive continue (vibrato, justesse) ;
        //   - 21 ms mini AUX CHANGEMENTS DE NOTE : mesure sur voix réelle, c'est
        //     ce qui fait passer les sauts brusques de 512 à 385 sans dégrader
        //     la justesse. Au-delà de 21 ms la note ne se pose plus (46 ms ->
        //     9 cents, 133 ms -> 11 cents).
        const bool noteChanged = (stickyNote != lastTarget);

        const double floorMs = noteChanged ? std::max ((double) params.noteRampMs, 0.0) : 0.0;
        const double tau = std::max ((double) params.retuneMs, std::max (6.0, floorMs)) * 0.001;
        const double alpha = std::exp (-hopSec / std::max (tau, 1.0e-4));

        const double demandedHz = std::max (30.0, hz * centsToRatio (cents));
        logHzSm = alpha * logHzSm + (1.0 - alpha) * std::log (demandedHz);

        // ---- période réelle du signal (celle des grains, pas la cible) -------
        const double p = std::min (sr / 70.0, std::max (sr / 1000.0, sr / hz));
        periodSm += (p - periodSm) * (noteChanged ? 1.0 : 0.7);

        ratioTarget = centsToRatio (cents);
        ratioSm     = std::min (2.0, std::max (0.5,
                        std::exp (logHzSm) * periodSm / sr));

        lastTarget = stickyNote;
        mixTarget  = 1.0;
        voicedNow  = true;
        unvoicedFrames = 0;

        //  FENÊTRE D'ANALYSE ADAPTATIVE.
        //  Une fenêtre de 2,2 périodes donne un NSDF exploitable (au décalage
        //  égal à la période, il reste 1,2 période d'intégration) tout en
        //  ramenant le retard d'analyse à 5-20 ms au lieu de 29,4 ms. C'est ce
        //  qui permet au mode Live de corriger le son qu'il est en train de
        //  lire, et non celui d'il y a 30 ms.
        detector.setAnalysisWindow ((int) std::lround (2.2 * sr / std::max (hz, 1.0)));

        uiHz.store ((float) hz);
        uiMidi.store ((float) midiF);
        uiVoiced.store (true);
        uiTarget.store (stickyNote);
        uiCents.store ((float) ((midiF - (double) stickyNote) * 100.0));
    }
    else
    {
        // ---- non voisé : sibilante, plosive, souffle, silence ----------------
        //  On passe en direct (le moteur est court-circuité) et on relâche la
        //  correction en 15 ms pour ne pas créer de marche de hauteur.
        mixTarget = 0.0;
        voicedNow = false;

        //  Sans hauteur on garde la dernière fenêtre (une note qui reprend est
        //  ainsi détectée tout de suite), mais si le silence dure on revient à
        //  la fenêtre maximale : sinon une voix grave qui démarre après un
        //  passage aigu ne serait jamais trouvée (la fenêtre courte ne contient
        //  pas assez d'échantillons pour un décalage de 70 Hz).
        if (++unvoicedFrames > 20)
            detector.setAnalysisWindow (0);
        else if (lastHz > 0.0)
            detector.setAnalysisWindow ((int) std::lround (2.2 * sr / lastHz));

        const double alpha = std::exp (-hopSec / 0.015);
        logHzSm = alpha * logHzSm + (1.0 - alpha) * std::log (sr / periodSm);
        ratioSm = std::min (2.0, std::max (0.5, std::exp (logHzSm) * periodSm / sr));
        ratioTarget = 1.0;

        holdSamples = std::max (0, holdSamples - hop);

        uiHz.store (0.0f);
        uiVoiced.store (false);
        uiTarget.store (-1);
        uiCents.store (0.0f);
    }

    uiRatio.store ((float) ratioSm);
}

// ============================================================================
//  Traitement
// ============================================================================
void TuneCore::process (const float* const* in, float* const* out, int numSamples, int channels)
{
    if (numSamples <= 0 || ! ready || in == nullptr || out == nullptr)
        return;

    if (in[0] == nullptr || out[0] == nullptr)
        return;

    //  Un seul moteur pour tous les canaux : gauche et droite reçoivent
    //  exactement la même correction, un signal mono reste donc parfaitement
    //  mono. Si l'hôte envoie plus de canaux que prepare() n'en a vus, on borne
    //  et on duplique (voir LiveEngine::process).
    const int prepared     = std::max (1, numChannels);
    const int hostChannels = std::max (1, std::min (2, channels));
    const int used         = std::min (hostChannels, prepared);

    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (kChunk, numSamples - done);

        // ---- 1. décision de hauteur (tous les 128 échantillons) -------------
        const PitchDetector::Estimate e = detector.process (in[0] + done, n);
        updateDecision (e);

        // ---- 2. moteur -------------------------------------------------------
        //  Plus d'anneau de commande : la latence du moteur (2,7 ms) est plus
        //  courte que la demi-fenêtre d'analyse, donc la commande courante
        //  décrit bien le son que le moteur lit. Le réglage FORMANT n'a plus
        //  d'effet (une ligne à retard déplace les formants avec la hauteur) :
        //  il est conservé pour ne pas invalider les presets.
        LiveEngine::Control c;
        c.ratio  = ratioSm;
        c.period = periodSm;
        c.voiced = voicedNow;
        c.mix    = params.bypass ? 0.0
                                 : std::min (1.0, std::max (0.0, mixTarget * (double) params.mix));

        const float* ip[2] = { in[0] + done,
                               used > 1 ? in[1] + done : in[0] + done };
        float*       op[2] = { out[0] + done,
                               used > 1 ? out[1] + done : out[0] + done };

        engine.process (ip, op, n, used, c);
        done += n;
    }

    // ---- 4. gain de sortie ---------------------------------------------------
    const double g = dbToGain ((double) params.outputGainDb);

    if (std::abs (g - 1.0) > 1.0e-6)
        for (int c2 = 0; c2 < used; ++c2)
            for (int i = 0; i < numSamples; ++i)
                out[c2][i] = (float) ((double) out[c2][i] * g);

    // canaux non traités : duplication du dernier canal (pas de silence)
    for (int c2 = used; c2 < hostChannels; ++c2)
        std::memcpy (out[c2], out[used - 1], sizeof (float) * (size_t) numSamples);

    // ---- 5. vu-mètres entrée / sortie ---------------------------------------
    {
        double inPk = 0.0, inSq = 0.0, outPk = 0.0, outSq = 0.0;

        for (int c2 = 0; c2 < used; ++c2)
            for (int i = 0; i < numSamples; ++i)
            {
                const double x = (double) in[c2][i];
                inPk = std::max (inPk, std::abs (x));
                inSq += x * x;

                const double y = (double) out[c2][i];
                outPk = std::max (outPk, std::abs (y));
                outSq += y * y;
            }

        const double n = (double) std::max (1, numSamples * used);
        const double inRmsNow  = std::sqrt (inSq  / n);
        const double outRmsNow = std::sqrt (outSq / n);

        // attaque immédiate, relâchement doux (~300 ms)
        const double rel = std::exp (-(double) numSamples / (0.30 * sr));
        inPeakLin  = std::max (inPk,     inPeakLin  * rel);
        outPeakLin = std::max (outPk,    outPeakLin * rel);
        inRmsLin   = std::max (inRmsNow, inRmsLin   * rel);
        outRmsLin  = std::max (outRmsNow, outRmsLin * rel);

        uiInPeak.store  ((float) linToDb (inPeakLin));
        uiInRms.store   ((float) linToDb (inRmsLin));
        uiOutPeak.store ((float) linToDb (outPeakLin));
        uiOutRms.store  ((float) linToDb (outRmsLin));
    }
}

} // namespace tune
