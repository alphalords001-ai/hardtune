# État du projet — 2 novembre 2026

## Où en est le travail

**À toi de jouer à l'étape 3.** Tout ce qui pouvait être fait sans ta machine a été fait :
le moteur est écrit **et validé par la mesure**, l'interface est écrite, le projet compile
au niveau syntaxique contre JUCE 9.0.4. Il reste à le compiler sur ton Windows et — c'est le
moment important — à **l'écouter**.

---

## Étapes du plan (parcours allégé)

| # | Étape | État | Détail |
|---|---|---|---|
| 1 | Atelier prêt (VS2022, CMake, Git) | ⬜ **à toi** | `docs/BUILD-WINDOWS.md` §1-2 |
| 2 | Squelette du projet | ✅ fait | `CMakeLists.txt`, `PluginProcessor`, `PluginEditor` |
| 3 | Détection de hauteur | ✅ fait | YIN, précision mesurée **< 0,1 cent** |
| 4 | 🔒 Verrou 1 : ta voix | ⬜ après build | à valider à l'oreille |
| 5 | Gammes et notes cibles | ✅ fait | 8 gammes + gamme perso, 17 tests |
| 6 | Moteur de correction | ✅ fait | PSOLA, justesse mesurée **< 1 cent**, latence exacte |
| 7 | Réglages du son | ✅ fait | Retune, Amount, Transpose, Mix, Output, presets |
| 8 | Formant / Throat | ✅ fait (⚠️ à valider à l'oreille) | resampling des grains |
| 9 | 🔒 Verrou 2 : l'écoute | ⬜ après build | **le vrai juge de paix** |
| 10 | Paramètres et état | ✅ fait | APVTS, automation, sauvegarde projet |
| 11 | Interface complète | ✅ fait (⚠️ rendu non vérifié visuellement) | roue, knobs, écran, presets, DPI |
| 12 | Validation hôte | ⬜ après build | pluginval, ton DAW, CPU |
| 13 | 🔒 Verrou 3 : projet réel | ⬜ | un morceau complet |
| 14 | Publication open source | ⬜ | `LICENSE` GPLv3 déjà en place |
| 15 | Installeur + CI | ⬜ | après validation |
| 16 | Branche Ubuntu | ⬜ | plus tard, ~2 jours |

Légende : ✅ fait et vérifié · ⚠️ fait mais non vérifiable sans ta machine · ⬜ à faire

---

## Ce que les tests automatiques garantissent (mesuré, pas supposé)

| Test | Résultat |
|---|---|
| Détection sur sinus 82 → 880 Hz | erreur **0,00 à 0,12 cent** |
| Correction d'un sinus désaccordé (227 → 233,08 Hz) | **0,0 cent** d'erreur |
| Correction d'une sawtooth « voix » (148 Hz) | **0,0 cent** |
| Correction vers le grave (225 → 220 Hz) | **0,0 cent** |
| Latence réelle vs annoncée | 1499 vs 1500 samples (**31 ms**) |
| Reconstruction à `amount = 0` | corrélation **0,9991** (transparence) |
| Résultat indépendant de la taille de bloc (64 / 512 / 2048) | identique |
| Bruit, silence, cas dégénérés | stable, pas de NaN, pas d'emballement |
| Gammes (7 notes, pentatoniques, blues, sensible…) | conformes |

Commande pour rejouer les tests : voir `README.md` (section « Tester le moteur sans DAW »).

---

## Points d'honnêteté technique

1. **Aucun son n'a encore été écouté.** Le DSP est juste sur le papier et à la mesure ; le
   rendu « T-Pain » se juge à l'oreille, à l'étape 9. C'est le seul vrai risque du projet.
2. **La GUI n'a pas été vue.** Elle compile, elle est structurée, mais aucun pixel n'a été
   affiché : il peut rester des ajustements de marges/tailles.
3. **Latence : ~31 ms** à 48 kHz (objectif du plan : ≤ 30 ms — on est à la limite). Elle est
   bornée par la note la plus grave gérée (80 Hz). Si tu chantes surtout haut, on peut la
   réduire (paramètre « plus basse note attendue »).
4. **Décalage résiduel du PSOLA** : le chemin corrigé est décalé de ≤ une demi-période
   par rapport au signal sec. Inaudible à 100 % wet ; avec **Mix < 100 %**, ça peut créer un
   léger creux (comportement normal pour ce type de moteur — Auto-Tune recommande aussi
   100 %).
5. **Transposition limitée à ±6 demi-tons** (au-delà, le ratio sort de la plage du moteur).
6. **Le splash screen JUCE** reste affiché : c'est une obligation de la licence GPLv3.

---

## Ce que je fais ensuite (dès que tu me réponds)

- Tu m'envoies le résultat du build (succès ou message d'erreur).
- Si ça compile : on passe à l'**étape 9, l'écoute critique**. Tu enregistres 3 prises
  (voix parlée, chant doux, chant fort), tu me dis ce qui te gêne, j'ajuste le DSP.
- Ensuite : interface (étape 11), puis packaging Windows (15), puis Ubuntu (16).

## Ce que tu fais maintenant

Voir **`docs/BUILD-WINDOWS.md`** : installer Visual Studio 2022 (charge de travail C++),
CMake et Git, puis double-cliquer sur `build.bat`.
Compte 30 à 45 minutes, dont 15 de compilation.
