//  PitchDetector.h — détection de hauteur par l'algorithme YIN.
//
//  Référence : A. de Cheveigné & H. Kawahara, "YIN, a fundamental frequency
//  estimator for speech and music", JASA 111(4), 2002.
//
//  Principe : fonction de différence + normalisation cumulative (CMNDF) +
//  seuil absolu + interpolation parabolique. Robuste sur la voix parlée/chantée,
//  temps de calcul raisonnable.
#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace tune
{

class PitchDetector
{
public:
    struct Estimate
    {
        double hz = 0.0;          // fréquence détectée (0 si silence)
        double confidence = 0.0;  // 0 = pas de périodicité, 1 = parfaitement périodique
        long long pos = 0;        // position absolue (en samples) de la fin de la fenêtre d'analyse
    };

    void prepare (double sampleRate, int windowSize = 1024, int hopSize = 128);
    void reset();

    void setRange (double minHz, double maxHz);

    /** Consomme un bloc. Les estimations disponibles sont ajoutées à `out`. */
    void process (const float* mono, int numSamples, std::vector<Estimate>& out);

    double getSampleRate() const noexcept { return sr; }
    int getWindowSize()  const noexcept { return windowSize; }

private:
    bool analyse (double& hz, double& confidence);

    double sr = 48000.0;
    int windowSize = 1024;
    int hopSize = 128;

    double minHz = 60.0, maxHz = 1200.0;

    std::vector<float>  tail;      // les `windowSize` derniers samples (le plus récent en dernier)

    //  Version decimee (2x) du signal, pour l'analyse seulement : deux fois
    //  moins d'echantillons a correler = jusqu'a 8x moins de calcul, sans
    //  perdre la duree d'analyse (le meme nombre de millisecondes).
    std::vector<float>  dec;
    std::vector<float>  carry;     // samples pas encore traités par hop
    std::vector<double> diff;      // fonction de différence
    std::vector<double> cmnd;      // fonction normalisée

    long long absPos = 0;
};

} // namespace tune
