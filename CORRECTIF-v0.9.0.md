# Correctif v0.9.1 — nouvelle interface paysage (maquette v0.6)

**Ce qui change : l'apparence.** Le moteur audio n'est pas touché : mêmes paramètres,
même DSP, mêmes réglages de référence.

## Les quatre fichiers sont enfin à leur place de compilation

| Fichier | Emplacement |
|---|---|
| `PluginEditor.cpp` / `.h` | `Source/` |
| `Theme.h` / `Widgets.h` | `Source/gui/` |

Déposés à la racine, ils n'étaient compilés par personne : `CMakeLists.txt` ne liste
que `Source/PluginEditor.cpp`.

## Corrections appliquées à la maquette

| Point | Maquette d'origine | v0.9.1 |
|---|---|---|
| `formantAtt` | `setupKnob (formantBar, …)` — `BarControl` n'est pas un `Knob` : **ne compilait pas** | `SliderAttachment` direct |
| FLEX-TUNE | éteint, affichait la valeur de HUMANIZE | câblé sur `amount`, affiche sa propre valeur |
| Clavier | deux masques codés à la main (`0xAAA`, `0xAB5`) | `tune::NoteMapper::scaleMask()` — les 9 gammes |
| Note jouée | jamais dessinée | `setPlaying()`, comparée en classes de hauteur absolues |
| Convention du clavier | blanches en absolu, noires décalées par la tonalité | tout en absolu, comme le moteur |
| LIGHT | libellé « LIGHT » attaché à `darkMode` → inversé | inversion gérée : allumé = mode clair |
| `cycleSize()` | déclarée, jamais définie → **erreur d'édition de liens** | définie : 0,75 / 1,0 / 1,5 par `setSize` + `setTransform` |
| Oscilloscope | `copyScope()` écrit 2048 valeurs dans un tampon de 512 → **écrasement mémoire** | tampon `kScopeSize`, seules les 512 dernières sont dessinées |
| `timerCallback` | aucune protection | `try/catch` |
| Titre | HARDTUNE | ALPHA-TUNE |
| Bouton ⤢ | `x = 1374`, dehors de la carte | `x = 1308`, dans la carte ; BYPASS décalé en 1190 |

## Ce qui n'a pas changé

- **FORMANT** reste affiché, grisé « (INACTIF) » : le paramètre existe mais n'a aucun
  effet audio. Choix conservé de la maquette.
- **OUTPUT** reste absent de l'interface (comme en v0.8.3). Le paramètre existe toujours
  pour l'automation.
- **RETUNE** garde son défaut réel : **0 ms** (plage 0 → 400 ms). 75 ms était un libellé
  d'attente de l'ancienne interface, pas la valeur du paramètre.
- Le commutateur PITCH CORRECTION reste absent (retiré en v0.8.1).

## Retour arrière

```
git revert <sha de ce commit>
```

L'interface précédente (820×600, v0.8.3) revient d'un seul coup : les quatre fichiers
sont remplacés, aucun autre fichier du projet n'a bougé.

Pour ne revenir que sur le clavier : `git checkout <sha>~1 -- Source/gui/Widgets.h`.

## Vérifications faites avant build

- Identifiants de paramètres utilisés : `retune`, `amount`, `humanize`, `vibrato`,
  `vibratoRate`, `formant`, `transpose`, `darkMode`, `bypass`, `key`, `scale` — tous présents.
- `cycleSize()` définie (l. 342) et appelée (l. 74).
- `copyScope()` : tampon de `kScopeSize` (2048), indexation `kScopeSize - 512`.
- `KeyStrip::inScale` et `pc` des noires en classes absolues ; `isPlaying` aligné.
- Compilation validée par le workflow `Build`.
