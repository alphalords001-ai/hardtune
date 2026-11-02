# Mise au point du projet — Plugin de correction de pitch (style « hard tune »)

**Version du document :** 1.0 — 2 novembre 2026
**Statut :** à valider par le porteur du projet avant toute écriture de code

---

## 1. Objectif du projet

Créer un **plugin d'effet audio** de correction de hauteur (pitch correction) en temps réel,
pensé pour produire le rendu « hard tune » popularisé par T-Pain : correction très rapide,
effet assumé, robotique, tout en restant musical.

Le plugin doit fonctionner **nativement** sur :

| Plateforme | Format | Priorité |
|---|---|---|
| Windows 10/11 (64 bits) | VST3 | **Phase 1** |
| Ubuntu 22.04 / 24.04 (64 bits) | VST3 | Phase 2 |

Une même base de code, deux compilations. Pas de version « dégradée » sous Linux.

---

## 2. Ce que fait le plugin (périmètre v1)

### 2.1 Inclus

- Correction de pitch d'une **voix monophonique** (chant, rap, talk-over), en temps réel.
- Détection automatique de la hauteur, sans MIDI, avec quantification sur une gamme choisie.
- Réglage de la **vitesse de correction** (de « instantané / hard tune » à « naturelle »).
- Réglage de la **quantité de correction** (0 % = son d'origine, 100 % = note totalement calée).
- Réglage du **formant / throat** pour garder ou exagérer le timbre.
- **Transposition** en demi-tons, mix dry/wet, gain de sortie, bypass.
- Interface graphique complète : roue de tonalités, gammes, indicateur de note détectée,
  vumètre de justesse en cents, gros boutons rotatifs.
- Presets d'usine dont un preset **« T-PAIN »** (son signature en un clic).
- Sauvegarde/restauration de l'état dans le projet du DAW, paramètres automatisables.

### 2.2 Volontairement exclu de la v1

| Exclu | Raison |
|---|---|
| Polyphonie / accords | Change complètement l'algorithme, coût x5 |
| Version AAX (Pro Tools) | SDK nécessite un accord développeur payant |
| Version AU / macOS | Hors périmètre demandé, ajout facile plus tard |
| Mode « MIDI in » (jouer la note cible au clavier) | v2 |
| Détection de tonalité automatique (auto-key) | v2 |
| Licence en ligne / protection anti-copie | v2, seulement si commercialisation |
| Synthèse d'harmonies, vocoder, effets | Hors sujet |

> **Point important :** on ne clone pas Antares Auto-Tune. On produit le **même type d'effet**.
> Aucune marque, aucun nom déposé, aucune ressource graphique d'Antares ne sera utilisée.

---

## 3. Spécifications fonctionnelles détaillées

### 3.1 Paramètres exposés à l'utilisateur

| # | Paramètre | Plage | Défaut | Rôle |
|---|---|---|---|---|
| 1 | Key (tonalité) | C … B | C | Fondamentale de la gamme |
| 2 | Scale (gamme) | 8 gammes | Major | Notes autorisées |
| 3 | Custom notes | 12 interrupteurs | — | Gamme personnalisée |
| 4 | Retune Speed | 0 → 800 ms | 20 ms | 0 = hard tune instantané, haut = naturel |
| 5 | Amount (correction) | 0 → 100 % | 100 % | Dosage de la correction |
| 6 | Formant / Throat | −100 → +100 | 0 % | Timbre, « épaisseur » de la voix |
| 7 | Transpose | −24 → +24 demi-tons | 0 | Décalage global |
| 8 | Humanize | 0 → 100 % | 0 % | Laisse passer les micro-variations (v1.1) |
| 9 | Mix | 0 → 100 % | 100 % | Dry/Wet |
| 10 | Output | −24 → +12 dB | 0 dB | Gain de sortie |
| 11 | Bypass | on/off | off | Comparaison A/B |

C'est le jeu de paramètres qui sera **figé avant le code** : chaque paramètre implique du DSP,
du GUI, de l'automation et de la compatibilité de presets. En ajouter un plus tard = migration.

### 3.2 Gammes disponibles v1

Chromatique, Majeur, Mineur naturel, Mineur harmonique, Pentatonique majeur,
Pentatonique mineur, Blues, Dorien. (+ gamme personnalisée par 12 interrupteurs.)

### 3.3 Presets d'usine

`T-PAIN` (retune 0 ms, amount 100 %, formant +15) — `TRAP` — `RAP` — `NATUREL` —
`CHŒUR` (retune lent, amount 60 %) — `INIT`.

### 3.4 Interface graphique

Structure de l'écran (format paysage, redimensionnable, style sombre/néon) :

```
┌──────────────────────────────────────────────────────────────┐
│  LOGO            [ Preset ▾ ]        [A/B]  [BYPASS]         │
├───────────────┬──────────────────────────┬───────────────────┤
│  Roue des 12  │   ◯ Retune   ◯ Amount    │  NOTE DÉTECTÉE    │
│  notes (C…B)  │   ◯ Formant  ◯ Mix       │  (grand affichage)│
│  + octave     │                          │  aiguille ±50 cents│
│  Gamme ▾      │                          │  VU d'entrée      │
│  Clavier 12   │                          │                   │
│  touches      │                          │  Transpose / Out  │
└───────────────┴──────────────────────────┴───────────────────┘
```

### 3.5 Exigences de qualité audio

| Critère | Cible v1 |
|---|---|
| Latence annoncée au DAW | ≤ 30 ms (compensation automatique par l'hôte) |
| Plage de détection | 60 Hz → 1 000 Hz (voix parlée à voix de tête) |
| Erreur de détection | < 10 cents sur signal tenu |
| Octave errors (erreurs d'octave) | < 2 % des frames, corrigées par lissage |
| Stabilité | aucun clic, aucun dépassement > 0 dBFS en sortie |
| CPU | < 5 % d'un cœur moderne par instance |

---

## 4. Spécifications techniques

### 4.1 Choix de la chaîne de fabrication

| Élément | Choix | Pourquoi |
|---|---|---|
| Framework | **JUCE 8** | Le plus simple nativement, VST3 Windows+Linux, GUI intégrée |
| Langage | C++17 | Standard de l'écosystème audio |
| Build | **CMake** + Ninja | Un seul `CMakeLists.txt` pour les deux OS |
| Format | VST3 (+ Standalone pour tester vite) | Standard actuel, multiplateforme |
| Compilateur Windows | MSVC (Visual Studio 2022) | Officiel Steinberg/JUCE |
| Compilateur Linux | GCC (Ubuntu 22.04 en conteneur) | Compatibilité glibc maximale |
| GUI | JUCE natif + LookAndFeel maison | Aucune dépendance externe |

**Licence — DÉCIDÉ : projet open source → JUCE GPLv3 (gratuit).**

Conséquences actées :

- Le projet est publié sous **GPLv3** : fichier `LICENSE`, code source complet disponible.
- Aucune licence JUCE à payer, **aucune signature de code nécessaire** (pas de certificat OV/EV).
- Distribution possible via **GitHub Releases** (`.zip` du `.vst3` + installeur).
- Le SDK VST3 (Steinberg) est également compatible GPLv3 : les deux licences cohabitent sans problème.
- Si un jour le projet devient un produit fermé, il faudra **racheter une licence JUCE commerciale**
  et retirer le code publié : c'est un changement lourd, à garder en tête avant de publier.
- Bonus open source : les utilisateurs peuvent compiler eux-mêmes pour Linux/ARM, ce qui allège
  notre travail de packaging sur Ubuntu.

### 4.2 Architecture du code (découpage prévu)

```
autotune/
├── CMakeLists.txt
├── Source/
│   ├── PluginProcessor.h/.cpp        ← paramètres, état, latence, buffer
│   ├── PluginEditor.h/.cpp           ← interface
│   ├── gui/
│   │   ├── LookAndFeel.h/.cpp        ← style visuel néon
│   │   ├── NoteWheel.h/.cpp          ← roue des 12 notes
│   │   ├── PitchMeter.h/.cpp         ← aiguille de justesse
│   │   └── Knob.h/.cpp               ← gros boutons rotatifs
│   └── dsp/
│       ├── PitchDetector.h/.cpp      ← algorithme YIN
│       ├── PsolaEngine.h/.cpp        ← moteur de décalage de hauteur
│       ├── NoteMapper.h/.cpp         ← gammes, note la plus proche
│       └── PitchTracker.h/.cpp       ← lissage, erreurs d'octave, voiced/unvoiced
├── tests/
│   └── dsp_offline.cpp               ← tests numériques hors DAW
├── packaging/
│   ├── windows/installer.iss         ← Inno Setup
│   └── linux/build-deb.sh
└── docs/
    ├── BUILD-WINDOWS.md
    ├── BUILD-UBUNTU.md
    └── UTILISATION.md
```

**Règle d'or :** tout le DSP est écrit en C++ **sans dépendance JUCE**, pour pouvoir être
testé en ligne de commande, sans DAW, sans GUI. C'est ce qui permet de garantir la justesse
du moteur au lieu de « régler à l'oreille ».

### 4.3 Chaîne de traitement audio

```
Entrée → buffer de retard (look-ahead) → détection de pitch (YIN, toutes les 256 samples)
      → lissage + choix de la note cible (gamme) → calcul du ratio de correction
      → moteur PSOLA (décalage de hauteur + formants)
      → mix dry/wet → gain de sortie → Sortie
```

### 4.4 Latence : le compromis central

Un autotune temps réel **ne peut pas être à latence zéro** : il doit « voir » au moins une
période de la voix avant de pouvoir la corriger. Ordre de grandeur :

| Élément | Latence ajoutée |
|---|---|
| Fenêtre d'analyse (1024 samples @ 44,1 kHz) | ~23 ms |
| Look-ahead du moteur | ~12 ms |
| **Total annoncé au DAW** | **~25–35 ms** |

C'est comparable à Auto-Tune en mode temps réel. **À valider par toi** : si tu comptes chanter
en direct avec le retour casque, 30 ms est acceptable ; si tu veux du monitoring ultra-serré,
il faudra accepter plus d'artefacts (fenêtre plus courte). Ce choix conditionne tout le DSP.

---

## 5. Découpage du travail (phases)

Chaque phase se termine par un **livrable testable** — on ne passe pas à la suivante sans l'avoir validé.

| Phase | Contenu | Livrable | Durée estimée |
|---|---|---|---|
| **P0** | Validation de ce document, choix licence, nom | Doc signé, décisions prises | — |
| **P1** | Squelette JUCE/CMake qui compile et sort un `.vst3` sous Windows, son non modifié | `.vst3` qui s'ouvre dans Reaper | 1 j |
| **P2** | Détection de pitch + affichage de la note détectée dans le GUI | GUI qui affiche les notes justes en chantant | 1–2 j |
| **P3** | Moteur PSOLA + quantification sur gamme → **le son hard tune** | Plugin qui « tune » pour de vrai | 2–4 j |
| **P4** | GUI complète (roue, gammes, knobs, presets, état, automation) | Interface finie, presets | 2–3 j |
| **P5** | Validation en conditions réelles (Reaper, DAW cible, pluginval, CPU) | Rapport de tests | 1–2 j |
| **P6** | Packaging Windows : installeur, chemins VST3, signature | `installer.exe` | 1 j |
| **P7** | Portage Ubuntu : conteneur, compatibilité glibc, `.deb`, tests Bitwig/Ardour/Carla | `.deb` + `.so` validé | 1–2 j |
| **P8** | Finitions : formants, optimisation CPU, jeux de presets | v1.0 | variable |

**Total réaliste : 3 à 4 semaines de travail effectif**, hors temps d'attente de tes retours.

### Jalons de décision (go / no-go)

- **Fin P1** : est-ce que le `.vst3` s'ouvre et s'entend dans ton DAW ? Sinon on corrige l'outillage avant tout le reste.
- **Fin P3** : est-ce que **le son te plaît** ? C'est le vrai point de non-retour. Si le rendu ne convainc pas, on ne maquille pas avec le GUI : on retravaille le DSP ou on change d'algorithme (PSOLA → phase vocoder, ou librairie Rubber Band).
- **Fin P5** : est-ce que ça tient dans un vrai projet avec 10 pistes et automation ?

---

## 6. Risques identifiés et parades

| Risque | Impact | Parade |
|---|---|---|
| **Qualité du son « pas assez T-Pain »** | Élevé | Test d'écoute à chaque itération ; jouer sur le formant ; option moteur alternatif |
| **Latence trop haute pour chanter en direct** | Moyen | Fenêtre adaptative ; mode « live » avec compromis assumé |
| **Erreurs d'octave sur voix soufflée/rap** | Moyen | Lissage + détection voiced/unvoiced + hystérésis |
| **Temps CPU de la détection YIN** | Faible | Réduction de la fenêtre, calcul par FFT si besoin |
| **Licence JUCE (GPL vs commerciale)** | **Bloquant** | Décision en P0 |
| **Signature Windows (SmartScreen)** | Moyen | Certificat OV/EV, ou acceptation au lancement (gratuit) |
| **Compatibilité glibc Ubuntu** | Faible | Build en conteneur Ubuntu 22.04 |
| **Nom du produit / marque** | Faible | Nom libre de droits, pas de référence à Auto-Tune |
| **Le projet reste « à moitié »** | Élevé | Phases courtes, livrables testables, aucun code avant P0 |

---

## 7. Ce que j'ai besoin de toi (bloquant)

1. **Licence** : perso gratuit (=> GPLv3, code publié) ou produit fermé/commercial (=> licence JUCE payante) ?
2. **Nom du plugin** : proposition de mon côté, mais c'est ta décision finale.
3. **Environnement de développement Windows** : Visual Studio 2022 est-il installé ? Sinon je fournis la procédure d'installation complète.
4. **DAW de test cible** : lequel utilises-tu réellement (FL Studio, Cubase, Reaper, Bitwig…) ? Cela détermine les tests de validation.
5. **Usage en direct** : veux-tu chanter avec le plugin en monitoring (contrainte de latence forte) ou uniquement traiter des pistes enregistrées (contrainte faible) ?
6. **Priorité** : qualité sonore maximale ou latence minimale, si les deux entrent en conflit ?

---

## 8. Ce qui est livré à la fin

- Le projet source complet, commenté, versionné.
- Un **`CMakeLists.txt` unique** produisant le `.vst3` sous Windows et sous Ubuntu.
- Un installeur Windows (`.exe`) et un paquet Ubuntu (`.deb` / `.tar.gz`).
- Les guides : `BUILD-WINDOWS.md`, `BUILD-UBUNTU.md`, `UTILISATION.md`.
- Le jeu de presets dont le preset « T-PAIN ».
- Les tests hors DAW (`tests/dsp_offline`) pour vérifier la justesse sans écouter.

---

## 9. Prochaine étape

Dès que tu réponds aux 6 points du **§7** (et que tu valides ou corriges les §§2–4),
je passe en **P1** : squelette qui compile, avec la procédure Windows pas à pas.

*Aucune ligne de code n'est écrite avant cet accord — c'est la règle.*
