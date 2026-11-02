# HardTune

Correcteur de hauteur temps réel (VST3) pour Windows et Linux — l'effet « hard tune »
assumé, à la T-Pain, avec gamme, vitesse de correction et formants.

> **État : v0.1 — moteur validé numériquement, premier build Windows à faire.**
> Voir `ETAT-DU-PROJET.md` pour le suivi précis.

---

## Fonctionnalités

- Correction de pitch monophonique en temps réel (voix chantée / parlée / rap)
- 12 tonalités × 8 gammes + gamme personnalisée (12 interrupteurs)
- **Retune Speed** 0 → 800 ms (0 = hard tune instantané)
- **Amount** 0 → 100 % (dosage de la correction)
- **Formant / Throat** −100 → +100 % (timbre, sans changer la note)
- Transposition ±6 demi-tons, Mix dry/wet, gain de sortie, bypass
- Interface : roue de tonalités, écran de note détectée, aiguille de justesse en cents
- 6 presets d'usine dont **T-PAIN**
- Latence annoncée au DAW (~31 ms à 48 kHz), compensée automatiquement par l'hôte

## Compiler

**Windows** → voir `docs/BUILD-WINDOWS.md` (puis double-clic sur `build.bat`).

**Linux** (étape ultérieure) → `./build.sh`.

Le premier build télécharge JUCE 9.0.4 automatiquement : aucun SDK à installer à la main.

## Tester le moteur sans DAW

```bash
g++ -O2 -std=c++17 -ISource/dsp tests/dsp_offline.cpp \
    Source/dsp/PitchDetector.cpp Source/dsp/TuneCore.cpp -o dsp_tests && ./dsp_tests
```

Ce harnais vérifie la détection (YIN), la quantification sur les gammes, la justesse de la
correction, la latence réelle, la transparence à `amount = 0`, la stabilité numérique et
l'indépendance vis-à-vis de la taille de bloc.

## Structure

```
Source/
├── PluginProcessor.*        enveloppe JUCE, paramètres, état, latence
├── PluginEditor.*           interface
├── gui/
│   ├── HardTuneLookAndFeel.*   identité visuelle (sombre / néon)
│   └── PitchDisplay.*          note détectée + aiguille de cents
└── dsp/                    ← C++ pur, AUCUNE dépendance JUCE
    ├── PitchDetector.*     détection YIN
    ├── NoteMapper.h        gammes, note cible
    ├── TuneCore.*          quantification + synthèse PSOLA
    └── TuneMath.h
tests/dsp_offline.cpp       tests numériques hors DAW
```

Le moteur DSP ne dépend pas de JUCE : il se teste en ligne de commande, ce qui permet de
vérifier la justesse par la mesure et non « à l'oreille ».

## Comment ça marche (résumé)

1. **Détection** — YIN sur une fenêtre de 1024 samples, toutes les 128 samples ;
   précision mesurée < 0,1 cent sur signal tenu.
2. **Quantification** — hauteur → note MIDI flottante → note autorisée la plus proche de la
   gamme choisie, avec hystérésis pour éviter les sauts entre deux notes.
3. **Correction** — ratio de fréquence, lissé dans le temps selon *Retune Speed* et pondéré
   par *Amount*.
4. **Synthèse PSOLA** — grains pitch-synchrones (marques de pitch recalées par corrélation),
   placés au pas de la période cible : la hauteur change, les formants restent.
   Les passages non voisés (souffle, consonnes) sont renvoyés en signal sec.

## Licence

**GPLv3** — voir `LICENSE`. Le projet utilise JUCE sous licence GPLv3 (gratuite pour les
projets open source) : si le code est distribué, il doit rester sous GPLv3.
