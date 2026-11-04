# HardTune

Correcteur de hauteur temps réel (VST3) pour Windows et Linux — l'effet « hard tune »
assumé, à la T-Pain, avec gamme et vitesse de correction, à 2,7 ms de latence.

> **État : v0.8.1 — moteur Live à 2,7 ms, commutateur PITCH CORRECTION retiré.**
>
> Le commutateur PITCH CORRECTION a été retiré en v0.8.1 : il ne coupait que
> l'accord et laissait vibrato et humanize actifs. Procédure de retour arrière :
> `docs/REVENIR-SUR-LE-RETRAIT.md`.
> Voir `ETAT-DU-PROJET.md` pour le suivi précis.

---

## Fonctionnalités

- Correction de pitch monophonique en temps réel (voix chantée / parlée / rap)
- 12 tonalités × 8 gammes + gamme personnalisée (12 interrupteurs)
- **Retune Speed** 0 → 800 ms (0 = hard tune instantané)
- **Amount** 0 → 100 % (dosage de la correction)
- ~~**Formant / Throat**~~ — **inactif depuis la v0.8.0** : un moteur à ligne à retard
  déplace les formants avec la hauteur. Le paramètre est conservé pour les presets.
- Transposition ±6 demi-tons, Mix dry/wet, gain de sortie, bypass
- Interface : roue de tonalités, écran de note détectée, aiguille de justesse en cents
- 6 presets d'usine dont **T-PAIN**
- Latence annoncée au DAW (~2,7 ms à 48 kHz), compensée automatiquement par l'hôte

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
    ├── PitchDetector.*     détection MPM/NSDF, fenêtre adaptative
    ├── NoteMapper.h        gammes, note cible
    ├── TuneCore.*          quantification + décision de hauteur
    ├── LiveEngine.*        ligne à retard à lecture pitch-synchrone
    └── TuneMath.h
tests/dsp_offline.cpp       tests numériques hors DAW
```

Le moteur DSP ne dépend pas de JUCE : il se teste en ligne de commande, ce qui permet de
vérifier la justesse par la mesure et non « à l'oreille ».

## Comment ça marche (résumé)

1. **Détection** — MPM/NSDF, analyse toutes les 128 samples, sur une fenêtre
   dimensionnée à 2,2 périodes de la note suivie (5 à 20 ms) : précision mesurée
   < 0,1 cent sur signal tenu.
2. **Quantification** — hauteur → note MIDI flottante → note autorisée la plus proche de la
   gamme choisie, avec hystérésis pour éviter les sauts entre deux notes.
3. **Correction** — ratio de fréquence, lissé dans le temps selon *Retune Speed* et pondéré
   par *Amount*.
4. **Lecture pitch-synchrone** — le passé est lu à la vitesse `2^(cents/1200)`.
   Quand le point de lecture a dérivé d'un cycle, on saute exactement une période
   (recherchée par corrélation, fondue sur une fraction de période). Plus de
   tampon de grains, donc plus de retard : la correction s'applique tout de suite.
   Les passages non voisés (souffle, consonnes) sont lus à la vitesse de l'entrée,
   donc rendus tels quels.

## Licence

**GPLv3** — voir `LICENSE`. Le projet utilise JUCE sous licence GPLv3 (gratuite pour les
projets open source) : si le code est distribué, il doit rester sous GPLv3.
