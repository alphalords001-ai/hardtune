//  PitchDetector.h — détection de hauteur par la méthode MPM (NSDF).
//
//  Remplace YIN (v0.7.0). Référence :
//      P. McLeod & G. Wyvill, "A smarter way to find pitch", ICMC 2005.
//
//  Principe : on calcule la fonction NSDF (normalized square difference
//  function), qui vaut 1 pour un décalage correspondant exactement à une
//  période et reste positive sur les décalages harmoniques — contrairement à la
//  fonction de différence de YIN qui, elle, est un « puits ». On choisit le
//  PREMIER maximum local qui dépasse 90 % du maximum global (règle MPM) : c'est
//  ce qui évite de détecter l'octave en dessous, défaut classique de YIN.
//
//  Coût : l'analyse est faite sur le signal décimé par 4 (12 kHz à 48 kHz), ce
//  qui divise le travail par 16 par rapport à une autocorrélation pleine bande,
//  puis la période est AFFINÉE sur le signal pleine bande (7 corrélations
//  seulement) pour retrouver une précision inférieure au cent.
//
//  Aucune allocation mémoire après prepare() : la fonction process() n'appelle
//  ni new, ni malloc, ni push_back.
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
        double hz         = 0.0;   // 0 = pas de hauteur exploitable
        double confidence = 0.0;   // clarté NSDF, 0..1
        double rms        = 0.0;   // niveau du bloc analysé
        double zcr        = 0.0;   // taux de passages par zéro (sibilantes)
        bool   voiced     = false; // périodicité suffisante
        bool   sibilant   = false; // bruit non harmonique (« S », « T », « P »)
        bool   valid      = false; // une nouvelle analyse a eu lieu pendant l'appel
        long long pos     = 0;     // position absolue de la fin de la fenêtre
    };

    void prepare (double sampleRate, int windowSize = 1024, int hopSize = 128);
    void reset();

    /** Plage de recherche du F0. 70 Hz = voix de basse, 1000 Hz = soprano. */
    void setRange (double minHz, double maxHz);

    /**
        Consomme un bloc. Renvoie la dernière estimation disponible.
        `valid` vaut false tant que le pas d'analyse n'a pas été atteint :
        l'appelant conserve alors sa décision précédente (aucun travail inutile).
    */
    Estimate process (const float* mono, int numSamples);

    double getSampleRate() const noexcept { return sr; }
    int    getWindowSize() const noexcept { return windowSize; }   // en échantillons réels
    int    getHopSize()    const noexcept { return hopSize; }

private:
    bool   analyse (double& hz, double& clarity);
    double refineLag (double coarseLagFull) const;
    void   buildDecimationFilter();

    double sr         = 48000.0;
    int    windowSize = 1024;      // durée d'analyse, en échantillons réels
    int    hopSize    = 128;

    double minHz = 70.0, maxHz = 1000.0;

    // --- décimation ---------------------------------------------------------
    static constexpr int kDecim = 4;
    static constexpr int kTaps  = 15;      // filtre anti-repliement (fenêtre de Blackman)
    float  fir[kTaps] {};
    int    nd      = 0;                    // taille de la fenêtre décimée
    int    tauMax  = 0;                    // décalage maximal (en échantillons décimés)
    int    tauMin  = 0;

    std::vector<float>  tail;      // fenêtre glissante pleine bande (le plus récent en dernier)
    float               carry[kDecim] {};  // échantillons pas encore décimés
    int                 carryLen = 0;
    long long           absPos   = 0;

    std::vector<float>  dec;       // signal décimé (nd échantillons)
    std::vector<double> nsdf;      // 0..tauMax
    std::vector<double> powPrefix; // sommes de carrés cumulées (dénominateur NSDF)
};

} // namespace tune
