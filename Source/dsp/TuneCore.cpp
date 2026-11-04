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

    // Tant que tout n'est pas pret, process() ne doit rien faire : un etat a
    // moitie construit (tampon vide, stretcher nul) donnerait un pointeur nul
    // au premier bloc audio.
    ready = false;

    try
    {
        auto newStretcher = std::make_unique<RubberBand::RubberBandStretcher> (
            (size_t) std::lround (sr),
            (size_t) numChannels,
            kOptions,
            1.0,        // time ratio (aucun changement de durée)
            1.0);       // pitch scale

        newStretcher->setMaxProcessSize ((size_t) maxBlock);
        newStretcher->setTimeRatio (1.0);
        newStretcher->setPitchScale (1.0);
        newStretcher->setFormantScale (1.0);

        //  Latence REELLE du stretcher (documentation Rubber Band) :
        //      getStartDelay()        = echantillons a jeter au debut de la sortie
        //      getPreferredStartPad() = zeros a mettre au debut de l'entree
        //  Nos anciennes versions n'annoncaient que le premier terme (1280) alors
        //  que le retard reel mesure hors DAW etait de 2048 echantillons : le DAW
        //  ne compensait donc que 26.7 ms sur 42.7, et la voix traitee arrivait
        //  systematiquement en retard sur le reste du projet.
        //  Surplus constate et reproductible : Rubber Band annonce 1280
        //  echantillons (getStartDelay) mais son retard RÉEL mesure hors DAW
        //  est de 2048 a 48 kHz -- verifie avec Rubber Band SEUL, sans notre
        //  code (chirp 200->400 Hz, correlation 1.0000). Il vaut donc mieux
        //  annoncer la valeur mesuree : sous-declarer la latence fait arriver
        //  la voix en retard sur le reste du projet.
        const double surplusReel = 768.0 * (sr / 48000.0);
        const int newLatency = (int) newStretcher->getStartDelay() + (int) std::lround (surplusReel);

        // Plancher 100 Hz (G2) : la fenetre reste a 1024 echantillons (21 ms).
        // A 80 Hz elle montait a 1500 echantillons (31 ms), et le retard de
        // detection mangeait la moitie de la correction du vibrato.
        detector.setRange (100.0, 1200.0);
        //  Pas d'analyse = 256 echantillons (5.3 ms) et non 128 : mesure hors
        //  DAW, cela fait passer le cout CPU du plugin de 15.7 % a 10.2 % d'un
        //  coeur pour seulement +4 cents de vibrato residuel. Le fil audio
        //  n'deborde plus, donc plus de craquements.
        detector.prepare (sr, 1024, 2 * kHop);

        // retard du chemin sec = latence du stretcher, arrondie à une puissance de 2
        int size = 1;
        while (size < newLatency + maxBlock + 64) size <<= 1;

        std::vector<float> newDry ((size_t) size * (size_t) numChannels, 0.0f);
        std::vector<float> newWet ((size_t) maxBlock * 2 + 2, 0.0f);
        const int newFifoSize = newLatency + 8 * maxBlock + 4096;
        std::vector<float> newFifo ((size_t) newFifoSize * (size_t) numChannels, 0.0f);

        // --- publication (aucune allocation apres ce point) -----------------
        stretcher = std::move (newStretcher);
        latency   = newLatency;
        dryMask   = size - 1;
        dryDelay  = std::move (newDry);
        tmpWet    = std::move (newWet);
        wetFifo   = std::move (newFifo);
        fifoSize  = newFifoSize;
        fifoRead  = 0;
        fifoFill  = 0;

        frames.clear();
        frames.reserve (512);
        estimates.reserve (64);

        reset();
        ready = true;
    }
    catch (...)
    {
        // echec (memoire, initialisation de Rubber Band...) : moteur inactif,
        // le plugin reste transparent au lieu de faire tomber l'hote.
        stretcher.reset();
        dryDelay.clear();
        tmpWet.clear();
        dryMask = 0;
        latency = 0;
        ready   = false;
    }
}

void TuneCore::reset()
{
    if (stretcher != nullptr)
        stretcher->reset();

    std::fill (dryDelay.begin(), dryDelay.end(), 0.0f);
    std::fill (wetFifo.begin(), wetFifo.end(), 0.0f);
    fifoRead = 0;
    fifoFill = 0;
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
    if (numSamples <= 0 || ! ready || stretcher == nullptr || in == nullptr || out == nullptr)
        return;

    if (in[0] == nullptr || out[0] == nullptr)
        return;

    // Les tampons doivent avoir exactement la taille annoncee par prepare() :
    // un etat incoherent ici veut dire pointeur nul ou debordement plus loin.
    if (maxBlock < 1
        || tmpWet.size() < (size_t) maxBlock * 2
        || dryDelay.size() < (size_t) (dryMask + 1)
        || (int) dryDelay.size() < (dryMask + 1) * std::max (1, std::min (2, numChannels)))
        return;

    // Combien de canaux le moteur peut-il vraiment traiter ?
    // dryDelay et le stretcher sont dimensionnes par prepare(). Si l'hote
    // envoie plus de canaux que prevu (instance mono preparte puis relancee en
    // stereo, changement de bus, wrappers de DAW...), ecrire sans borne
    // debordait du tas et faisait tomber le DAW. On borne, on traite, puis on
    // duplique le dernier canal traite sur les canaux restants.
    const int prepared     = std::max (1, numChannels);
    const int hostChannels = std::max (1, std::min (2, channels));
    const int used         = std::min (hostChannels, prepared);

    // Un bloc plus grand que ce que prepare() a annonce deborderait tmpWet :
    // on le decoupe en tranches de maxBlock (l'hote a le droit d'envoyer
    // n'importe quelle taille de bloc).
    int done = 0;
    while (done < numSamples)
    {
        const int n = std::min (maxBlock, numSamples - done);
        processChunk (in, out, done, n, used);
        done += n;
    }

    for (int c = used; c < hostChannels; ++c)
        std::memcpy (out[c], out[used - 1], sizeof (float) * (size_t) numSamples);
}

void TuneCore::processChunk (const float* const* in, float* const* out, int offset,
                             int numSamples, int channels)
{
    if (numSamples <= 0)
        return;

    // ---- 1. chemin sec : on retarde l'entrée de la latence du stretcher -----
    for (int c = 0; c < channels; ++c)
    {
        const float* src = in[c] + offset;
        float* dst = dryDelay.data() + (size_t) c * (size_t) (dryMask + 1);
        for (int i = 0; i < numSamples; ++i)
            dst[(int) ((writePos + i) & dryMask)] = src[i];
    }

    //  Le stretcher a ete cree avec `numChannels` canaux : il faut lui fournir
    //  exactement ce nombre de pointeurs. On traite les canaux du stretcher, en
    //  repetant le dernier canal reel si l'hote en envoie moins.
    const int stretcherCh = std::max (1, std::min (2, numChannels));

    // ---- 2. a 5. par petits pas de kHop (128 samples = 2.7 ms) -------------
    //  Avant, la hauteur etait mise a jour UNE fois par bloc : avec des blocs
    //  de 1024 dans le DAW, la correction arrivait jusqu'a 21 ms en retard et le
    //  vibrato n'etait corrige qu'a moitie. Mesures hors DAW : mise a jour tous
    //  les 128 samples = deux fois moins de vibrato residuel.
    int consumed = 0;

    while (consumed < numSamples)
    {
        const int n = std::min (kHop, numSamples - consumed);

        // ---- detection de hauteur (canal 1) --------------------------------
        estimates.clear();
        detector.process (in[0] + offset + consumed, n, estimates);

        for (const auto& e : estimates)
            pushFrame (e.pos + consumed, e.hz, e.confidence);

        // ---- mise a jour du moteur -----------------------------------------
        stretcher->setPitchScale (currentRatio());
        stretcher->setFormantScale (formantRatio());

        // ---- envoi au stretcher --------------------------------------------
        const float* inPtrs[2] = { in[0] + offset + consumed,
                                   channels > 1 ? in[1] + offset + consumed
                                                : in[0] + offset + consumed };
        stretcher->process (inPtrs, (size_t) n, false);

        // ---- recuperation : on vide TOUT dans la FIFO -----------------------
        //  Ne prendre qu'un bloc par bloc laissait le surplus du demarrage
        //  definitivement en retard (latence reelle 2048 au lieu de 1280).
        for (;;)
        {
            const int avail = (int) stretcher->available();
            if (avail <= 0)
                break;

            const int space = fifoSize - (fifoRead + fifoFill);
            if (space <= 0)
                break;                       // FIFO pleine : on reprendra au bloc suivant

            const int take = std::min (avail, space);

            float* chans[2] = { nullptr, nullptr };
            for (int c = 0; c < stretcherCh; ++c)
                chans[c] = wetFifo.data() + (size_t) c * (size_t) fifoSize
                                         + (size_t) (fifoRead + fifoFill);

            stretcher->retrieve (chans, (size_t) take);
            fifoFill += take;
        }

        consumed += n;
    }

    // ---- le bloc sort de la FIFO (zeros uniquement pendant l'amorcage) ------
    const int have = std::min (numSamples, fifoFill);

    for (int c = 0; c < stretcherCh; ++c)
    {
        float* dst = tmpWet.data() + (size_t) c * (size_t) maxBlock;
        const float* src = wetFifo.data() + (size_t) c * (size_t) fifoSize + (size_t) fifoRead;

        std::copy (src, src + have, dst);

        for (int i = have; i < numSamples; ++i)
            dst[i] = 0.0f;
    }

    fifoRead += have;
    fifoFill -= have;

    // compactage : la FIFO reste petite, le memmove est rare
    if (fifoRead > fifoSize / 2)
    {
        for (int c = 0; c < stretcherCh; ++c)
            std::memmove (wetFifo.data() + (size_t) c * (size_t) fifoSize,
                          wetFifo.data() + (size_t) c * (size_t) fifoSize + (size_t) fifoRead,
                          sizeof (float) * (size_t) fifoFill);

        fifoRead = 0;
    }

    totalIn += numSamples;

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
            out[c][offset + i] = (float) ((d + (w - d) * m) * g);
        }
    }

    writePos += numSamples;

    // ---- 6 bis. vu-mètres entrée / sortie ----------------------------------
    {
        double inPk = 0.0, inSq = 0.0, outPk = 0.0, outSq = 0.0;

        for (int c = 0; c < channels; ++c)
        {
            const float* src = in[c] + offset;
            for (int i = 0; i < numSamples; ++i)
            {
                const double x = (double) src[i];
                inPk = std::max (inPk, std::abs (x));
                inSq += x * x;

                const double y = (double) out[c][offset + i];
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
