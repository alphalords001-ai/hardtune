# Correctif v0.9.0 — nouvelle interface (maquette v0.6) intégrée

**Ce qui change : l'apparence.** Le moteur audio n'est pas touché : mêmes paramètres,
même DSP, mêmes réglages de référence. Seul l'écran change.

## La maquette est reprise, avec ses défauts corrigés

| Point | v0.6 (fichiers déposés) | v0.9.0 |
|---|---|---|
| Emplacement | 4 fichiers à la racine, compilés par personne | `Source/PluginEditor.*` + `Source/gui/Theme.h` + `Source/gui/Widgets.h` |
| `formantBar` | passé à `setupKnob()` qui attend un `ht::Knob&` → **ne compilait pas** | `SliderAttachment` direct |
| FLEX-TUNE | éteint, affichait la valeur de HUMANIZE | câblé sur `amount`, affiche sa propre valeur |
| Clavier | 2 masques codés à la main (`0xAAA`, `0xAB5`), `minor = (scaleIdx == 1)` | `tune::NoteMapper::scaleMask()` — les 9 gammes |
| Note jouée | jamais dessinée (`setPlaying()` non appelé) | surlignée |
| Convention du clavier | blanches en absolu, noires décalées par la tonalité | tout en absolu, comme le moteur |
| FORMANT | barre grisée « (INACTIF) » | retiré de l'interface |
| LIGHT / DARK | étiquette « LIGHT » sur `darkMode` → inversé | paire LIGHT · DARK, l'actif en surbrillance |
| Redimensionnement | `setResizable` + coordonnées absolues → écran disloqué | échelle réelle : `place()` et `paint()` multiplient par `uiScale` |
| Taille | 1408×768 imposé | ouverture à l'échelle qui tient sur l'écran, redimensionnable à ratio fixe |
| Oscilloscope | `copyScope()` écrit 2048 valeurs dans un tampon de 512 → **débordement de pile** | tampon de `kScopeSize`, puis les 512 dernières |
| `timerCallback` | aucune protection | `try/catch` + journal `hardtune-diag.txt` (comme en v0.8.3) |
| Titre | HARDTUNE | ALPHA-TUNE |

## Ce qui n'a pas changé

- `OUTPUT` reste absent de l'interface (comme en v0.8.3). Le paramètre existe toujours
  pour l'automation ; à trancher plus tard.
- Le commutateur PITCH CORRECTION reste absent (retiré en v0.8.1).
- Le moteur, le fondu de mixage de 12 ms et les tests DSP sont inchangés.

## Retour arrière

```
git revert <sha de ce commit>
```

L'ancienne interface (`820×600`, retour à la v0.8.3) revient d'un seul coup : les quatre
fichiers sont remplacés, aucun autre fichier du projet n'a bougé.

Pour revenir uniquement sur le clavier, sans toucher au reste :
`git checkout <sha>~1 -- Source/gui/Widgets.h`.

## Vérifications faites

- Les identifiants de paramètres utilisés existent tous (`retune`, `amount`, `humanize`,
  `vibrato`, `vibratoRate`, `transpose`, `darkMode`, `bypass`, `key`, `scale`).
- `formant` n'est plus référencé par l'interface ; le paramètre reste dans le processeur.
- Compilation validée par le workflow `Build`.
