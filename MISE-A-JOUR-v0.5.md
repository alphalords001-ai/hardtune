# HardTune v0.5.0 — la nouvelle interface est dans le code

Tout ce que tu as validé sur les images est maintenant **écrit en C++**, avec les
4 réglages qui manquaient au moteur.

---

## 1. Ce qui change pour toi

| Avant | Maintenant |
|---|---|
| ancienne interface | **celle de la maquette** : en-tête HARDTUNE, 12 touches, forme d'onde, gros cadran Ø350, colonne de droite, 2 vu-mètres |
| 6 boutons | **8 réglages** : ajout de HUMANIZE, VIBRATO, RATE et du bouton PITCH CORRECTION |
| pas de vu-mètres | **INPUT / OUTPUT** avec barres Peak et RMS réelles |
| pas de thème | **LIGHT / DARK** (le piano ne change jamais) |
| textes en français | **tout en anglais** |

---

## 2. Les fichiers à déposer dans ton dépôt GitHub

Dépose-les **en respectant les dossiers** (glisser le dossier `Source` va
remplacer les anciens fichiers, c'est voulu) :

```
CMakeLists.txt                     (à remplacer)
Source/PluginProcessor.h           (à remplacer)
Source/PluginProcessor.cpp         (à remplacer)
Source/PluginEditor.h              (à remplacer)
Source/PluginEditor.cpp            (à remplacer)
Source/dsp/TuneCore.h              (à remplacer)
Source/dsp/TuneCore.cpp            (à remplacer)
Source/gui/Theme.h                 (NOUVEAU)
Source/gui/Widgets.h               (NOUVEAU)
tests/dsp_offline.cpp              (à remplacer)
MISE-A-JOUR-v0.5.md                (NOUVEAU, ce fichier)
```

Rien d'autre ne bouge : `.github/workflows/build.yml`, `build.bat`, `LICENSE`
restent tels quels.

---

## 3. Ce qui est vérifié ici, dans la boîte

- les **tests du moteur** passent : `TOUS LES TESTS PASSENT (0 échec(s))`, **10 séries**.
  - série 9 (nouvelle) : *humanize fait dériver la hauteur* — écart mesuré 22 cents
  - série 10 (nouvelle) : *le vu-mètre d'entrée mesure la bonne crête* — 0,5 → **−6,0 dB**
- les 4 fichiers qui touchent à JUCE sont **compilés sans erreur** (contrôle de syntaxe
  contre JUCE 9.0.4, la version figée dans `CMakeLists.txt`).

Ce que je **ne peux pas** faire ici : lancer Windows. La compilation réelle se fait
sur GitHub, comme d'habitude.

---

## 4. Les 6 gestes sur Windows

1. Sur GitHub, dépose les fichiers de la section 2, puis **Commit changes**.
2. Onglet **Actions** → le point devient vert (≈ 8–12 min la première fois).
3. Clique sur le point vert → en bas, **HardTune-VST3-Windows** → télécharge.
4. Ouvre le ZIP téléchargé, copie le dossier `HardTune.vst3`.
5. Colle-le dans `C:\Program Files\Common Files\VST3`.
6. Dans FL Studio : `Options > Manage plugins > Find more plugins > Start scan`.

---

## 5. Ce que tu dois regarder en premier

1. La fenêtre **s'ouvre-t-elle à la bonne taille** (960 × 1050) ?
2. Le **clavier** réagit-il quand tu chantes (la touche visée s'allume) ?
3. Le **gros cadran** bouge-t-il (l'arc bleu et le chiffre au centre) ?
4. Les **2 vu-mètres** montent-ils quand tu chantes ?
5. Le bouton **LIGHT/DARK** change-t-il bien tout **sauf le piano** ?

Si un de ces 5 points ne marche pas, dis-moi **lequel** et je corrige.
