//  PsolaEngine.h — moteur de transposition temporelle TD-PSOLA.
//
//  Remplace Rubber Band (v0.7.0). Pourquoi :
//
//  Rubber Band est un vocodeur de phase : il travaille sur des fenêtres FFT et
//  impose de ce fait un retard FIXE de 2080 échantillons (43 ms) auquel
//  s'ajoutait notre FIFO, soit 56 ms annoncés au DAW. Ce retard n'est pas un
//  réglage : c'est la taille de la fenêtre. Conséquence mesurée sur une vraie
//  prise de voix : dès qu'on demande une correction douce (70 % / 20 ms), le
//  rendu tombe à 16 cents d'erreur, parce que la commande de hauteur arrive
//  toujours une fenêtre trop tard.
//
//  TD-PSOLA (Time-Domain Pitch Synchronous Overlap and Add) travaille
//  directement sur la forme d'onde :
//
//    - on découpe le signal en « grains » de DEUX périodes, pondérés par une
//      fenêtre de Hann ;
//    - chaque grain est recopié TEL QUEL (aucun rééchantillonnage) : son
//      contenu spectral est donc préservé, ce qui conserve les formants — c'est
//      la réponse au défaut « voix de dessin animé » ;
//    - on espace les grains de P/r à la sortie (r = rapport de hauteur) : c'est
//      cet espacement, et lui seul, qui produit la correction de hauteur ;
//    - les grains sont calés sur la période mesurée (on saute ou on répète une
//      période quand la dérive atteint une demi-période), ce qui garde les
//      harmoniques en phase d'un grain à l'autre.
//
//  Latence : elle ne dépend plus que de la géométrie des grains. Pour produire
//  l'échantillon t il faut le grain centré en t, donc l'entrée jusqu'à t + P.
//  D'où latency = 1,5 x Pmax + 2 x chunk, arrondie à la tranche : 1216
//  échantillons à 48 kHz (25,3 ms) et 1152 à 44,1 kHz (26,1 ms), au lieu de
//  2688 (56 ms) avec Rubber Band.
//
//  Aucune allocation mémoire dans process() : tous les tampons sont alloués par
//  prepare(), et le traitement est découpé en tranches de `chunk` échantillons
//  pour que le résultat ne dépende JAMAIS de la taille de bloc du DAW.
#pragma once

#include <vector>
#include <cstddef>

namespace tune
{

class PsolaEngine
{
public:
    struct Control
    {
        double ratio   = 1.0;   // > 1 = plus aigu
        double period  = 0.0;   // période mesurée, en échantillons (0 = inconnue)
        double formant = 1.0;   // échelle des formants (1 = préservés)
        double mix     = 1.0;   // 1 = traité, 0 = passe-through (non voisé)
    };

    void prepare (double sampleRate, int maximumBlockSize, int channels);
    void reset();

    int latencySamples() const noexcept { return latency; }

    /** Traite un bloc. in/out peuvent être identiques. Aucune allocation. */
    void process (const float* const* in, float* const* out, int numSamples,
                  int channels, const Control& c);

private:
    void   processChunk (const float* const* in, float* const* out, int offset,
                         int numSamples, int channels, Control c);
    void   writeGrain (long long center, double source, double period,
                       double halfWidth, int channels);
    float  readHist (int channel, double pos) const;
    double hann (double x) const;          // fenêtre de Hann, x dans [-1, 1]

    double sr = 48000.0;
    int    chunk = 64;
    int    preparedChannels = 1;
    int    latency = 0;

    double pMin = 48.0, pMax = 686.0;      // périodes extrêmes (1000 Hz .. 70 Hz)

    std::vector<float> hist;               // anneau d'entrée, par canal
    std::vector<float> ola;                // accumulation des grains, par canal
    std::vector<float> win;                // somme des fenêtres (normalisation)

    int histSize = 0, histMask = 0;
    int olaSize  = 0, olaMask  = 0;

    long long writePos  = 0;               // échantillons écrits
    long long emitPos   = 0;               // prochain slot de sortie produit
    double    nextCenter = 0.0;            // centre du prochain grain (temps absolu)
    double    srcPos     = 0.0;            // sa position source

    double    period    = 0.0;
    double    formantSm = 1.0;
    double    mixSm     = 0.0;
};

} // namespace tune
