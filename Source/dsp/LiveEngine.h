//  LiveEngine.h — correction de hauteur par ligne à retard à lecture
//  pitch-synchrone (v0.8.0).
//
//  Remplace PsolaEngine (v0.7.0). Pourquoi ce changement :
//
//  Le moteur PSOLA construisait des grains dont le centre était programmé
//  jusqu'à ce que `nextCenter + period` dépasse la sortie demandée. Comme le
//  centre du dernier grain tombait une période en arrière, la moitié de la
//  fenêtre s'écrivait sur des échantillons DÉJÀ LUS puis remis à zéro : la
//  correction n'arrivait qu'au tour suivant de l'anneau de sortie. Mesuré sur
//  une sinusoïde : la sortie restait à 220 Hz pendant 72 ms avant de sauter à
//  233,3 Hz d'un coup. C'est cette marche qui s'entendait comme « robotique ».
//
//  Ici il n'y a plus de grains du tout. On lit le passé à une vitesse
//  légèrement différente de celle à laquelle il arrive :
//
//      ratio = 2^(cents/1200)
//
//  - si ratio > 1, on lit PLUS VITE que l'écriture : on rattrape le présent ;
//  - si ratio < 1, on lit moins vite : on prend du retard.
//
//  Comme on finit toujours par rattraper (ou par traîner), le point de lecture
//  dérive. Quand il a dérivé d'un cycle, on saute exactement UNE période — en
//  arrière si on a rattrapé, en avant si on a traîné. Sauter d'une période
//  exacte sur un signal périodique est inaudible, et le saut est fondu sur une
//  fraction de période pour couvrir le cas où le signal change (voyelle qui
//  bouge, note qui commence).
//
//  Conséquences, toutes mesurées sur le banc tests/dsp_offline.cpp :
//
//    - la correction est appliquée à l'échantillon près, sans tampon à remplir :
//      la réponse du moteur ne dépend plus de la taille d'un anneau ;
//    - la latence ne dépend plus de la note : elle vaut `sr/400` plus une
//      constante de quelques échantillons, soit 2,7 ms à 44,1 kHz comme à
//      48 kHz, au lieu de 26 ms ;
//    - les formants SUIVENT la hauteur, comme sur une platine qu'on ralentit.
//      C'est le compromis assumé du mode Live : il n'y a plus de préservation
//      de formants. Le réglage FORMANT devient inactif.
//
//  Aucune allocation mémoire dans process() : tous les tampons sont alloués par
//  prepare(), et le traitement avance ÉCHANTILLON PAR ÉCHANTILLON, ce qui rend
//  le résultat indépendant de la taille de bloc du DAW.
#pragma once

#include <vector>
#include <cstddef>

namespace tune
{

class LiveEngine
{
public:
    struct Control
    {
        double ratio  = 1.0;    // > 1 = plus aigu
        double period = 0.0;    // période mesurée, en échantillons (0 = inconnue)
        double mix    = 1.0;    // 1 = traité, 0 = sec (non voisé)
        bool   voiced = false;  // une hauteur tenue est entendue
    };

    void prepare (double sampleRate, int maximumBlockSize, int channels);
    void reset();

    int latencySamples() const noexcept { return latency; }

    /** Traite un bloc. in/out peuvent être identiques. Aucune allocation. */
    void process (const float* const* in, float* const* out, int numSamples,
                  int channels, const Control& c);

    // ---- informations de diagnostic (bancs de mesure) ----------------------
    long long spliceCount()   const noexcept { return splices; }
    long long driftBackCount()const noexcept { return driftBacks; }

private:
    void   processChunk (const float* const* in, float* const* out, int offset,
                         int numSamples, int channels, const Control& c);
    float  readHist (int channel, double pos) const;
    void   maybeSplice (double ratio, double period, bool voiced);
    double findSpliceLength (double tapPos, int direction, double minLength,
                             double maxLength, double period, double& correlation) const;
    void   startSplice (double offset, double fadeSamples, double correlation);
    bool   driftBackInaudible (double delay) const;
    double periodicity (double period) const;

    double sr = 48000.0;
    int    preparedChannels = 1;
    int    latency   = 0;
    int    hopSize   = 118;
    int    maxPeriod = 686;
    int    unvoicedPeriod = 220;
    double pMin = 48.0, pMax = 686.0;

    double minDelay = 8.8;        // retard de lecture minimal (échantillons)
    double tap      = 0.0;        // point de lecture courant (position absolue)
    double dryTap   = 0.0;        // point de lecture du signal sec, aligné
    double spliceTap = 0.0;       // second point de lecture pendant un fondu

    // --- lissage du ratio, en cents ----------------------------------------
    double cents      = 0.0;
    double centsCoeff = 0.0;

    // --- fondu de saut en cours --------------------------------------------
    int    fadeLength = 0, fadePosition = 0;
    double fadeCorrelation = 1.0;

    // --- suivi --------------------------------------------------------------
    double trackedPeriod = 0.0;
    int    unvoicedRun   = 0;
    int    quietRun      = 0;
    double longPeak      = 0.0;
    double peakDecay     = 0.0;

    static constexpr int kTrackHops = 32;
    double trackPeriod[kTrackHops] {};
    bool   trackVoiced[kTrackHops] {};
    int    trackPos = 0, trackCount = 0;

    long long splices = 0, driftBacks = 0;

    // --- anneau d'entrée ----------------------------------------------------
    std::vector<float> hist;
    int histSize = 0, histMask = 0;
    long long writePos = 0;
};

} // namespace tune
