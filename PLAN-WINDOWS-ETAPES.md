# Plan d'exécution détaillé — Branche Windows
## Plugin de correction de pitch (hard tune) — VST3

**Version :** 1.0 — 2 novembre 2026
**Portée :** de la machine vierge au premier installeur Windows. La branche Ubuntu est traitée après (§10).
**Total : 42 étapes réparties en 7 lots — environ 15 jours de travail effectif, 3 semaines calendaires.**

> **Deux façons de lire ce document :**
> - le **parcours complet** (42 étapes, ci-dessous) : la version « usine », utile si on doit un jour
>   faire auditer le projet ou bosser à plusieurs ;
> - le **parcours allégé** (16 étapes, §13) : la même chose regroupée, sans étapes de test séparées.
>   C'est celui que je recommande pour démarrer. **Même charge de travail, moins de cases à cocher.**

---

## Comment lire ce document

Chaque étape comporte :

- **Qui** : `MOI` (je produis le code/fichier dans l'espace de travail), `TOI` (action sur ton PC), `ENSEMBLE` (je livre, tu testes et me renvoies le résultat).
- **Fait quand** : le critère de validation. Tant que ce critère n'est pas atteint, l'étape n'est pas terminée.
- Les durées sont en **jours de travail effectif**, pas en jours calendaires (les allers-retours entre nous allongent le calendrier).

Deux règles de conduite :

1. **Aucun lot n'est ouvert avant que le précédent soit validé.** Pas de GUI pendant que le son ne fonctionne pas.
2. **Chaque étape produit un artefact observable** : un `.vst3` qui s'ouvre, une note qui s'affiche, un chiffre dans un rapport. Jamais « ça devrait marcher ».

---

# LOT A — Préparation de l'atelier (1 jour)

Objectif : être capable de compiler *n'importe quel* plugin JUCE avant d'écrire le nôtre. On ne débogue jamais un outil et son propre code en même temps.

| # | Étape | Qui | Fait quand | Durée |
|---|---|---|---|---|
| **A1** | Installer **Visual Studio 2022 Community** avec la charge de travail « Développement Desktop en C++ » (MSVC v143 + Windows 11 SDK) | TOI | `cl.exe` répond dans le « Developer Command Prompt » | 0,5 j |
| **A2** | Installer **CMake ≥ 3.22**, **Ninja**, **Git for Windows** | TOI | `cmake --version`, `ninja --version`, `git --version` répondent | 0,2 j |
| **A3** | Décider la **licence** (GPLv3 gratuit vs commerciale payante) et le **nom du plugin** | TOI | Décision écrite dans ce document | — |
| **A4** | Je fournis un script `tools/build.bat` + la procédure de récupération de JUCE 8 (via `FetchContent`, rien à installer à la main) | MOI | Fichiers livrés | 0,1 j |
| **A5** | Compiler l'exemple officiel `juce_audio_plugin_demo` (plugin de démo JUCE, non modifié) | ENSEMBLE | Le `.vst3` de démo **s'ouvre et fait du bruit** dans ton DAW | 0,3 j |

> **Verrou A5.** Si la démo JUCE ne compile pas, on s'arrête et on règle l'outillage. Aucune ligne de code du projet avant ça.

---

# LOT B — Squelette du projet (1 jour)

Objectif : notre plugin existe, il s'ouvre dans le DAW, il laisse passer le son intact.

| # | Étape | Qui | Fait quand | Durée |
|---|---|---|---|---|
| **B1** | Arborescence du projet + `CMakeLists.txt` unique (formats VST3 + Standalone) | MOI | Fichiers livrés | 0,2 j |
| **B2** | `PluginProcessor` minimal : entrée → sortie sans traitement, paramètres vides | MOI | Compile sans warning | 0,2 j |
| **B3** | `PluginEditor` minimal : un rectangle avec le nom et un bouton bypass | MOI | Fenêtre qui s'ouvre | 0,1 j |
| **B4** | Compilation Release x64, copie du `.vst3` dans `C:\Program Files\Common Files\VST3` | TOI | Le plugin apparaît dans la liste des effets du DAW | 0,2 j |
| **B5** | Test d'écoute : une piste voix traverse le plugin sans altération | ENSEMBLE | Son identique à l'entrée, pas de clic, pas de latence visible | 0,2 j |

> **Verrou B5.** C'est le « hello world » du projet. Une fois vert : la chaîne de fabrication est fiable pour les 5 lots suivants.

---

# LOT C — Détection de hauteur (2 jours)

Objectif : savoir, à chaque instant, quelle note est chantée. C'est la moitié du travail : **une correction ne peut pas être meilleure que sa détection.**

| # | Étape | Qui | Fait quand | Durée |
|---|---|---|---|---|
| **C1** | Module `PitchDetector` (algorithme **YIN**) en C++ pur, sans JUCE | MOI | Écrit + compilable en ligne de commande | 0,5 j |
| **C2** | Harnais de test hors DAW `tests/dsp_offline` : génère des sinus/sawtooth à 82 / 220 / 440 / 880 Hz et mesure l'erreur de détection | MOI | Rapport chiffré (erreur en cents par fréquence) | 0,3 j |
| **C3** | Correction des cas limites : plage 60–1000 Hz, interpolation parabolique, seuil de voisement | MOI | Erreur < 10 cents sur signal tenu | 0,3 j |
| **C4** | `PitchTracker` : lissage, anti-erreur d'octave, distinction voisé / non voisé (souffle, silence, consonnes) | MOI | Pas de saut d'octave sur un test de glissando | 0,4 j |
| **C5** | Branchement dans le plugin + affichage temps réel de la note détectée (texte + aiguille en cents) | MOI | GUI qui bouge quand tu chantes | 0,3 j |
| **C6** | Test réel : tu chantes/parles 2 minutes, on relève les notes affichées | ENSEMBLE | Détection cohérente sur ta voix, sur au moins 3 timbres | 0,2 j |

> **Verrou C6.** Si la détection est fausse sur ta voix, on ne va pas plus loin dans le DSP. On ajuste d'abord.

---

# LOT D — Le son : correction « hard tune » (4 jours)

**C'est le cœur du projet. C'est ici que le son T-Pain se gagne ou se perd.**

| # | Étape | Qui | Fait quand | Durée |
|---|---|---|---|---|
| **D1** | Module `NoteMapper` : tables des 8 gammes, choix de la note la plus proche, calcul du ratio de correction (en demi-tons puis en ratio de fréquence) | MOI | Tests unitaires sur 12 tonalités × 8 gammes | 0,4 j |
| **D2** | `PsolaEngine` v1 : moteur de décalage de hauteur par grains (PSOLA), rapport fixe, sans formant | MOI | f0 mesurée en sortie = f0 attendue (± 5 cents) sur test hors DAW | 1,2 j |
| **D3** | Anti-artefacts : choix des marques de pitch, crossfade des grains, gestion des grains manquants | MOI | Spectrogramme propre, pas de clic sur signal tenu | 0,5 j |
| **D4** | Déclaration de latence au DAW (`setLatencySamples`) + compensation de retard | MOI | Le DAW compense, pas de décalage audible | 0,2 j |
| **D5** | Paramètre **Retune Speed** : lissage temporel de la correction (0 ms = instantané / hard tune) | MOI | Différence audible nette entre 0 ms et 300 ms | 0,3 j |
| **D6** | Paramètre **Amount** : dosage 0–100 % de la correction appliquée | MOI | À 0 % le traitement est transparent | 0,2 j |
| **D7** | **Knob Formant / Throat** : conservation ou exagération des formants | MOI | Le timbre change sans que la note change | 0,7 j |
| **D8** | Preset **« T-PAIN »** codé en dur : amount 100 %, retune 0 ms, formant +15 | MOI | Bouton qui produit le rendu signature | 0,1 j |
| **D9** | **Écoute critique** : tu chantes 3 prises (voix parlée, chant doux, chant fort), on juge le rendu | ENSEMBLE | Tu dis : « ça y est » ou « ce n'est pas ça » | 0,4 j |

> **Verrou D9 — JALON GO/NO-GO N°1.** Le point de non-retour du projet.
> - Si **oui** → on continue vers la GUI.
> - Si **presque** → on itère sur le DSP (paramétrage des grains, formants), 1 à 3 jours de plus.
> - Si **non** → on change de moteur (phase vocoder ou librairie Rubber Band) : +2 à 4 jours, mais on ne maquille pas un son raté avec une belle interface.

---

# LOT E — Interface complète et paramètres (3 jours)

Objectif : transformer un moteur audio en produit utilisable.

| # | Étape | Qui | Fait quand | Durée |
|---|---|---|---|---|
| **E1** | Migration des 11 paramètres en **APVTS** (automation, save/restore, noms, unités) | MOI | Tous les paramètres bougent par l'automation du DAW | 0,5 j |
| **E2** | `LookAndFeel` maison : fond sombre, néon, typographie, boutons rotatifs dessinés | MOI | Rendu visuel conforme à la maquette | 0,7 j |
| **E3** | Roue des 12 tonalités + sélecteur de gamme + clavier 12 touches (gamme personnalisée) | MOI | L'interface réagit et les paramètres suivent | 0,5 j |
| **E4** | Vumètre de justesse (aiguille ±50 cents), note détectée, VU d'entrée, indication de latence | MOI | Affichage fluide, sans clignotement, < 1 % CPU | 0,4 j |
| **E5** | Gestion des **presets** (6 presets d'usine + sauvegarde utilisateur) | MOI | Preset rappelé correctement après rechargement | 0,3 j |
| **E6** | État du plugin : fermeture/réouverture du projet DAW, rappel exact des réglages | MOI | Aucune perte de réglage | 0,2 j |
| **E7** | Redimensionnement, DPI 125/150 %, affichage multi-écrans | MOI | Interface nette sur ton écran réel | 0,4 j |

---

# LOT F — Validation en conditions réelles (2 jours)

| # | Étape | Qui | Fait quand | Durée |
|---|---|---|---|---|
| **F1** | Passage au validateur **pluginval** (niveau 10, strict) | TOI | 0 erreur | 0,2 j |
| **F2** | Test dans **ton DAW principal** : insertion, automation, copie de piste, 10 instances | ENSEMBLE | Aucun crash, aucune glitch audio | 0,5 j |
| **F3** | Test des cas tordus : changement de sample rate, buffer 32 / 2048, bypass en cours de lecture | ENSEMBLE | Comportement stable | 0,3 j |
| **F4** | Mesure CPU réelle sur ton PC (1 puis 10 instances) | TOI | < 5 % par instance | 0,2 j |
| **F5** | Test « projet réel » : 1 morceau complet avec le plugin dessus | TOI | Le rendu tient sur toute une session | 0,8 j |

> **Verrou F5 — JALON GO/NO-GO N°2.** Si ça tient dans un vrai projet, on package. Sinon on corrige avant de distribuer.

---

# LOT G — Packaging Windows (1,5 jour)

| # | Étape | Qui | Fait quand | Durée |
|---|---|---|---|---|
| **G1** | Icône, bannière, page « à propos », nom de société | TOI + MOI | Ressources fournies | 0,3 j |
| **G2** | Script **Inno Setup** : installe le `.vst3` dans `C:\Program Files\Common Files\VST3`, menu démarrer, désinstalleur | MOI | Fichier `.iss` livré | 0,3 j |
| **G3** | Compilation de l'installeur + test sur une **machine propre** (ou profil vierge) | TOI | Installation → plugin visible → désinstallation propre | 0,4 j |
| **G4** | Signature de code (optionnel) : certificat OV/EV pour éviter l'alerte SmartScreen | TOI | Décision + achat éventuel | — |
| **G5** | `README` d'installation pour l'utilisateur final + captures d'écran | MOI | Doc livrée | 0,5 j |

> Fin du **lot G = version Windows 1.0 livrable.**

---

## 10. Ensuite seulement : la branche Ubuntu

Résumé (à détailler dans un document séparé une fois Windows validé) :

| Lot | Contenu | Durée |
|---|---|---|
| **H1** | Conteneur Docker Ubuntu 22.04 + dépendances de compilation | 0,5 j |
| **H2** | Compilation du `.vst3` (dossier `Contents/x86_64-linux/`) | 0,3 j |
| **H3** | Vérification compatibilité glibc (`ldd`), linkage statique `libstdc++` | 0,3 j |
| **H4** | Tests dans Reaper / Bitwig / Ardour / Carla | 0,5 j |
| **H5** | Paquet `.deb` + archive `.tar.gz` avec script d'installation dans `~/.vst3` | 0,4 j |
| | **Sous-total** | **≈ 2 jours** |

Le code ne change pas (hors quelques `#if` de chemins et de fenêtrage). C'est le bénéfice d'avoir choisi un framework portable dès le lot A.

---

## 11. Récapitulatif du calendrier

| Lot | Contenu | Étapes | Jours |
|---|---|---|---|
| A | Préparation de l'atelier | 5 | 1,0 |
| B | Squelette qui compile et sonne | 5 | 1,0 |
| C | Détection de hauteur | 6 | 2,0 |
| D | **Le son hard tune** | 9 | 4,0 |
| E | Interface et paramètres | 7 | 3,0 |
| F | Validation réelle | 5 | 2,0 |
| G | Packaging Windows | 5 | 1,5 |
| | **Total Windows** | **42** | **≈ 14,5 j** |
| H | Branche Ubuntu (après) | 5 | 2,0 |

Avec les allers-retours (installation, tests d'écoute, corrections), compte **3 semaines calendaires pour Windows**, puis **3 à 4 jours pour Ubuntu**.

---

## 12. Les 4 moments où tout peut basculer

1. **A5** — la démo JUCE ne compile pas → problème d'outillage, pas de projet.
2. **C6** — la détection est fausse sur ta voix → le DSP entier est à revoir.
3. **D9** — **le son ne te plaît pas** → le seul risque vraiment sérieux du projet.
4. **F5** — ça craque dans un vrai projet → problème de robustesse, pas de fonctionnalité.

Tout le reste est du travail prévisible.

---

*Prochaine action attendue : validation de ce plan + réponses aux 6 questions bloquantes du document `PLAN-DE-TRAVAIL.md` (licence, nom, environnement, DAW, live/studio, priorité son/latence). Ensuite je livre le lot A.*

---

# 13. Parcours allégé — 16 étapes (recommandé)

## Pourquoi cette version existe

Le plan en 42 étapes ne contient **pas plus de travail** : il découpe finement. Ton objection est
juste sur un point : compter les tests comme des étapes séparées donne l'impression d'un chantier
administratif. Alors voilà la même chose regroupée.

Trois principes :

1. **Un test n'est pas une étape, c'est la définition de « terminé ».** « Le moteur PSOLA est fait »
   signifie déjà « la hauteur de sortie est vérifiée ». Pas besoin d'une étape test séparée.
2. **On ne garde que 3 verrous** (les 3 moments où l'on risque de perdre du temps à continuer dans
   une mauvaise direction). Le reste se vérifie en continu, dans le fil du travail.
3. **Ce qui peut être automatique le devient** (CI GitHub Actions) : plus de « est-ce que ça compile
   chez moi ? » — c'est la machine qui répond à chaque commit.

## Les 16 étapes

| # | Étape | Contenu regroupé | Fait quand | Durée |
|---|---|---|---|---|
| **1** | Atelier prêt | VS2022 + CMake + Ninja + Git, démo JUCE compilée et entendue | La démo JUCE sonne dans ton DAW | 1 j |
| **2** | Squelette | Projet, `CMakeLists.txt`, processor/editor minimal, `.vst3` installé | Le plugin s'ouvre et le son passe intact | 1 j |
| **3** | Détection de hauteur | YIN + lissage + affichage de la note et des cents | En chantant, la note affichée est la bonne | 2 j |
| **4** | 🔒 **Verrou 1** : ta voix | Tu testes 3 timbres, on corrige les cas ratés | Détection fiable sur ta voix | 0,5 j |
| **5** | Gammes et notes cibles | Tables des 8 gammes, note la plus proche, ratio de correction | Tests unitaires au vert | 0,5 j |
| **6** | Moteur de correction | PSOLA + anti-artefacts + latence déclarée au DAW | La hauteur de sortie est juste, sans clic | 1,5 j |
| **7** | Réglages du son | Retune Speed, Amount, Transpose, Mix, preset « T-PAIN » | Le rendu signature est là | 1 j |
| **8** | Formant / Throat | Conservation et exagération du timbre | Le timbre change, la note non | 0,7 j |
| **9** | 🔒 **Verrou 2** : l'écoute | 3 prises enregistrées, jugement à l'oreille | Tu dis « c'est ça » — sinon on itère | 0,5 j |
| **10** | Paramètres et état | Automation, presets, sauvegarde dans le projet DAW | Réglages rappelés après rechargement | 0,8 j |
| **11** | Interface complète | Look néon, roue des notes, knobs, vumètre, DPI | GUI finie et fluide | 3 j |
| **12** | Validation hôte | pluginval + ton DAW + 10 instances + CPU | 0 erreur, < 5 % CPU, aucun crash | 1,5 j |
| **13** | 🔒 **Verrou 3** : projet réel | Un morceau complet avec le plugin dessus | Ça tient sur toute une session | 1 j |
| **14** | Publication open source | `LICENSE` GPLv3, `README`, captures, `.zip` du `.vst3` | Dépôt public prêt, archive téléchargeable | 0,5 j |
| **15** | Installeur + CI | Inno Setup, test sur machine propre, GitHub Actions (build auto à chaque commit) | `.exe` qui s'installe et se désinstalle proprement | 1 j |
| | **Total Windows** | | | **≈ 14 j** |
| **16** | Branche Ubuntu | Conteneur, `ldd`, tests Reaper/Bitwig/Ardour, `.deb` | Le même `.vst3` fonctionne sous Ubuntu | 2 j |

## Ce qu'on ne supprime pas (et pourquoi)

Trois contrôles restent obligatoires, même dans la version allégée :

- **pluginval** : il détecte les crashs à l'ouverture/fermeture du plugin et les buffer sizes
  extrêmes. Le genre de bug qui n'apparaît jamais chez soi mais fait désinstaller un plugin.
- **Le test sur machine propre** : un plugin qui marche chez le développeur parce qu'il a une DLL
  oubliée dans son PATH est le classique du genre.
- **Les changements de sample rate / buffer** : 44,1 / 48 / 96 kHz et 32–2048 samples. C'est 20 minutes
  de test qui évitent 80 % des rapports de bug.

## Ce qui peut sauter sans risque

- Les dizaines de mesures chiffrées du lot C (erreur en cents, spectrogrammes) : utiles pendant
  le développement du moteur, inutiles à te faire valider séparément — je les garde pour moi.
- La signature de code : inutile en open source.
- Les tests multi-écrans/DPI avancés : on verra à l'usage.
- Le lot « F1 à F5 » en étapes distinctes : regroupé dans l'étape 12.

## Charge réelle, en clair

**≈ 14 jours de travail effectif** dans les deux présentations. La différence est uniquement
dans la façon de raconter le projet : 16 grandes étapes lisibles, ou 42 micro-étapes traçables.

