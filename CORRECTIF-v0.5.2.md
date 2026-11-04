# v0.5.2 — blindage de l'interface et du moteur

Le rapport de crash de FL Studio (v0.5.1) était décisif :

```
Exception: Access violation at address 00007FFCA32C5346 in module 'HardTune.vst3'
           (offset D5346). Read of address 0000000000000000
Callstack: HardTune.vst3 (3 frames) → USER32.dll
```

Trois enseignements :

1. le crash n'est **plus** dans JUCE ni dans le moteur lié à l'ouverture : le
   premier défaut (fermeture au chargement) est corrigé ;
2. `Read of address 0000000000000000` = **déréférencement de pointeur nul** ;
3. la pile passe par `USER32.dll` = **fil d'interface** (traitement d'un message
   Windows), et non le fil audio.

## Ce qui pouvait donner un nul côté interface

**`timerCallback` lisait les paramètres sans vérifier le pointeur.** C'était le
seul pointeur brut de l'interface :

```cpp
proc.apvts.getRawParameterValue (HardTuneProcessor::kRetune)->load()   // avant
```

`getRawParameterValue` renvoie `nullptr` si l'identifiant n'est pas (ou plus)
dans la table : état restauré d'une autre version, remplacement de l'arbre de
paramètres, ou paramètre désinscrit. Le timer tourne 30 fois par seconde et
remplit des libellés **dès que de l'audio arrive** — d'où un crash qui
n'apparaît qu'avec du son.

Deuxième piège de même nature : les composants dessinés utilisaient des tailles
non bornées. `Gauge::paint` fait `font (d * 0.165f)` où `d` est la taille du
composant ; avec un composant de taille nulle, la hauteur de police vaut 0, JUCE
renvoie alors un `Typeface` nul, et dessiner du texte avec un typeface nul est
un… « Read of address 0x0 ».

## Correctifs

**`Source/PluginEditor.cpp` / `.h`**
- `timerCallback` ne fait plus rien d'autre qu'appeler `refreshFromProcessor()`
  sous `try/catch` : aucune exception ne peut remonter dans la boucle de
  messages de l'hôte.
- Toutes les lectures de paramètres passent par `paramValue (id, valeur_par_defaut)`,
  qui vérifie le pointeur au lieu de le déréférencer.

**`Source/gui/Theme.h`, `Source/gui/Widgets.h`**
- `ht::font()` borne la hauteur entre 5 et 400 px : plus jamais de typeface nul.
- `Knob`, `Gauge`, `KeyStrip`, `Scope`, `LevelMeter` ne dessinent plus rien si le
  composant est trop petit (taille nulle pendant la création de fenêtre, un
  changement de DPI, une mise à zéro par l'hôte…).

**`Source/dsp/TuneCore.*`**
- nouveau drapeau `ready` : `process()` ne fait rien tant que `prepare()` n'a pas
  entièrement réussi.
- `prepare()` est exception-safe : toutes les allocations sont faites **avant**
  de publier l'état ; en cas d'échec, le moteur reste inactif (plugin
  transparent) au lieu de laisser un état à moitié construit.
- `process()` vérifie la cohérence des tampons (taille, `maxBlock`, canaux) avant
  d'écrire quoi que ce soit.

**`Source/dsp/PitchDetector.cpp`**
- `process()` refuse de tourner sans `prepare()` : le `memmove` partait sinon
  d'un pointeur nul.

**`Source/PluginProcessor.cpp`**
- `getCallbackLock()` partagé entre `prepareToPlay` et `processBlock` : les
  tampons ne peuvent plus être réalloués pendant qu'un bloc audio les lit.
- `getStateInformation` / `setStateInformation` sous `try/catch`.

## Vérifications

- Compilation `-Wall -Wextra` sans erreur (JUCE 9.0.4 + Rubber Band 4.0.0).
- 10 séries de tests DSP : **0 échec**.
- AddressSanitizer + UndefinedBehaviorSanitizer, 6 cas d'hôte hostiles : propres.
- Cas limites supplémentaires : `process()` sans `prepare()`, `prepare(0,0,0)`,
  tampon nul, 0 sample → **tous sans effet, aucun plantage**.

## Si ça plante encore

Il faut la pile complète. FL Studio l'a déjà écrite :

```
C:\Users\ALPHA Prod\Documents\Image-Line\FL Studio\Support\Logs\Crash\
```

Ouvre le fichier le plus récent de ce dossier : il contient la pile complète avec
les modules de chaque frame, ce qui permet de voir si l'appel vient du timer, du
dessin, du clic, ou du fil audio.
