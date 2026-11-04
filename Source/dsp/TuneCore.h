//  TuneCore.h — moteur de correction de hauteur (v0.7.0).
//
//  Chaîne complète :
//
//      PitchDetector (MPM/NSDF, 70-1000 Hz)
//            -> décision de hauteur : note visée, tolérance, lissage
//            -> PsolaEngine (TD-PSOLA, grains calés sur la période)
//
//  Ce que la v0.7.0 change par rapport à la v0.6.2 :
//
//    - Rubber Band (vocodeur de phase, 56 ms de retard) est remplacé par un
//      moteur temporel TD-PSOLA : 25,3 ms à 48 kHz (26,1 ms à 44,1 kHz),
//      annoncés au DAW et mesurés (écart < 2,5 ms) ;
//    - YIN est remplacé par MPM/NSDF : même précision, pas d'erreur d'octave
//      par construction (on prend le PREMIER pic, pas le puits le plus profond) ;
//    - les segments non voisés (sibilantes « S », plosives « T »/« P »,
//      souffle) passent en direct, sans passer par le moteur : plus d'artefacts
//      métalliques sur les consonnes ;
//    - les formants sont préservés par construction (les grains sont recopiés
//      tels quels, jamais rééchantillonnés) ;
//    - plus aucune allocation mémoire dans process() : tout est alloué dans
//      prepare().
#pragma once

#include <vector>
#include <atomic>
#include <cstddef>

#include "PitchDetector.h"
#include "NoteMapper.h"
#include "TuneMath.h"
#include "PsolaEngine.h"

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

        // --- v0.7.0 : réglages internes, sans identifiant d'automation ------
        //  (aucun paramètre existant n'est renommé : les presets restent valides)
        float noteRampMs  = 21.0f;   // rampe appliquée UNIQUEMENT aux changements de note
        float sibilantMs  = 20.0f;   // maintien de la correction après une consonne
    };

    TuneCore();
    ~TuneCore();

    void prepare (double sampleRate, int maximumBlockSize, int numChannels = 1);
    void reset();

    void setParams (const Params& p) noexcept  { params = p; }
    NoteMapper& notes() noexcept               { return mapper; }

    /** Traite un bloc (in et out peuvent pointer sur les mêmes canaux). */
    void process (const float* const* in, float* const* out, int numSamples, int numChannels);

    int latencySamples() const noexcept        { return engine.latencySamples(); }

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
    /** Une trame de décision, produite à chaque analyse du détecteur. */
    void   updateDecision (const PitchDetector::Estimate& e);
    double humanizeCents();

    // --- configuration ------------------------------------------------------
    double sr = 48000.0;
    int    maxBlock = 512;
    int    numChannels = 1;
    std::atomic<bool> ready { false };
    Params params;

    // --- moteurs ------------------------------------------------------------
    PitchDetector detector;
    PsolaEngine   engine;

    // --- décision de hauteur ------------------------------------------------
    NoteMapper mapper;
    int    stickyNote  = 60;
    bool   stickyValid = false;
    int    lastTarget  = -1;
    double lastHz      = 0.0;    // dernière hauteur acceptée (garde de continuité)
    double candHz      = 0.0;    // valeur en attente de confirmation
    int    candCount   = 0;
    double ratioTarget = 1.0;
    double ratioSm     = 1.0;
    double logHzSm     = 0.0;    // logarithme de la hauteur de SORTIE visée
    double periodSm    = 0.0;
    double mixTarget   = 0.0;
    double amountSm    = 1.0;
    int    holdSamples = 0;

    // --- retard de commande -------------------------------------------------
    //  Le détecteur mesure la hauteur du son d'il y a une demi-fenêtre ; le
    //  grain en cours de fabrication est, lui, en arrière de `latence`. On
    //  applique donc la commande créée `controlDelaySamples` échantillons plus
    //  tôt : c'est cette commande-là qui décrit le son que le moteur fabrique.
    std::vector<double> ctrlRatio;
    std::vector<double> ctrlPeriod;
    int ctrlMask         = 0;
    int ctrlPos          = 0;
    int ctrlDelayChunks  = 0;

    // --- humanize / vibrato -------------------------------------------------
    unsigned int rngState = 0x12345678u;
    int    humanizeTicks = 0;
    double humanizeWalk  = 0.0;
    double humanizeTarget= 0.0;
    double vibratoPhase  = 0.0;

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
