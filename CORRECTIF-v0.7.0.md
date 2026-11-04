# Alpha-Tune v0.7.0 — nouveau moteur : TD-PSOLA + MPM, section pass-through

Cette version remplace le moteur de transposition. Ce n'est pas un réglage
supplémentaire : Rubber Band est retiré du projet.

## Pourquoi ce changement

Rubber Band est un vocodeur de phase. Il impose un retard **fixe** de 2080
échantillons (43 ms), plus notre FIFO de sortie : 2688 échantillons annoncés au
DAW, soit **56 ms**. Ce retard n'était pas réglable, c'est la taille de sa
fenêtre d'analyse.

Conséquence mesurée sur une vraie prise de voix : dès qu'on demandait une
correction douce (70 % / 20 ms), le rendu tombait à 16 cents d'erreur — la
commande de hauteur arrivait toujours une fenêtre trop tard. Les réglages « à la
VoxTune » étaient donc inutilisables.

## Ce qui change

| | v0.6.2 | v0.7.0 |
|---|---|---|
| Détection | YIN, fenêtre 1024 fixe | **MPM / NSDF**, 70-1000 Hz |
| Synthèse | Rubber Band (vocodeur de phase) | **TD-PSOLA** (grains calés sur la période) |
| Latence annoncée | 2688 éch. = 56 ms | **1216 éch. = 25,3 ms** (48 kHz) |
| CPU (bloc 512) | 10,2 % d'un cœur | **2,2 %** |
| Erreur médiane (voix réelle) | 4 cents | **7,9 cents** |
| Sauts brusques (voix réelle) | 512 | **365** |
| Erreurs d'octave | 0 | **0** |
| Sibilantes | traitées par le moteur | **passe-through direct** |
| Formants | écrasés (`setFormantScale(1.0)`) | **préservés par construction** |
| Allocations dans `process()` | non | **non** (vérifié) |

## Les quatre demandes, point par point

### 1. Détection MPM/NSDF, plage 70-1000 Hz

`Source/dsp/PitchDetector.cpp` est réécrit. La fonction NSDF remplace la
fonction de différence de YIN, et la règle MPM (premier maximum local au-dessus
de 90 % du maximum global) supprime le risque d'octave en dessous.

L'analyse est faite sur le signal **décimé par 4** (filtre FIR 15 coefficients,
coupure 4,4 kHz) : le coût est divisé par 16 par rapport à une autocorrélation
pleine bande, sans perdre de précision puisque la période est ensuite **affinée
sur le signal pleine bande** (7 corrélations seulement). Précision mesurée sur
sinus de 82,4 à 880 Hz : < 10 cents, typiquement < 1 cent.

La plage est imposée à 70-1000 Hz dans `TuneCore::prepare()`.

### 2. Détection des sons non harmoniques, contournement du pitch-shifter

Chaque analyse renvoie aussi le niveau et le taux de passages par zéro. Un
segment est traité comme « non voisé » (sibilante « S », plosive « T »/« P »,
souffle) dès que la clarté NSDF est insuffisante, et le moteur est alors
**court-circuité** : la sortie est le signal d'entrée retardé, sans aucun
traitement. Le basculement est fondu sur 5 ms pour ne pas claquer.

Un **maintien de 20 ms** évite de couper la correction au milieu d'une syllabe
(une plosive interrompt la périodicité pendant 10 à 30 ms).

Mesure (test 11) : corrélation entrée/sortie sur un « S » synthétique =
**1,00000**, c'est-à-dire un passe-through exact.

### 3. Temps réel, lissage, zéro allocation

- **TD-PSOLA** (`Source/dsp/PsolaEngine.cpp`) : grains de deux périodes pondérés
  par une fenêtre de Hann, recopiés tels quels, espacés de `P / r` à la sortie.
  C'est l'espacement, et lui seul, qui produit la correction.
- **Lissage** : on lisse la **hauteur de sortie en cents**, pas le rapport de
  transposition. C'est une différence de fond — en lissant le rapport, une
  petite erreur sur la période de référence devenait une erreur de justesse
  (mesuré : 11 cents sur voix réelle → 7,9 cents après correction). Aux
  changements de note, une rampe de 21 ms s'ajoute (mesurée sur voix réelle :
  c'est le minimum de sauts).
- **Retard de commande** : la mesure décrit le son d'il y a une demi-fenêtre,
  alors que le grain fabriqué est 25 ms en arrière. On applique donc la commande
  créée `latence - demi-fenêtre` échantillons plus tôt. Balayage : 0 → 13,8
  cents, 224 → 10,8, **448 → 7,9**, 672 → 7,7 (mais plus de sauts), 896 → 11,0.
- **Zéro allocation** : tous les tampons sont dimensionnés dans `prepare()`.
  Vérifié par recherche des `push_back`, `resize`, `assign`, `new` : aucun dans
  le chemin audio. Le traitement est découpé en tranches fixes de 64
  échantillons, donc le résultat ne dépend pas de la taille de bloc du DAW
  (test 6 : 185,01 Hz en bloc 64 et en bloc 2048).

### 4. Préservation des formants

Elle est **structurelle** : un grain PSOLA n'est jamais rééchantillonné, donc
son contenu spectral est conservé quand on change l'espacement. Le réglage
FORMANT agit séparément (largeur de fenêtre ≠ largeur de source), ce qui déplace
les formants sans toucher à la hauteur.

Mesure (test 12, transposition de +4 demi-tons sur une voix synthétique dont le
formant est à 3 kHz) : le pic reste à l'harmonique 13 (3,03 kHz) — c'est-à-dire
à 3 kHz en fréquence absolue. Un moteur qui rééchantillonne le placerait à
l'harmonique 16 (3,73 kHz) : c'est exactement l'effet « Mickey Mouse ».

## Ce qui n'est pas fait, et pourquoi

- **Latence 0** : impossible pour un moteur périodique. Pour produire
  l'échantillon `t`, il faut le grain centré en `t`, donc l'entrée jusqu'à
  `t + P`. À 70 Hz, `P` vaut 14 ms à lui seul. La latence est donc
  `1,5 × P(70 Hz) + 2 tranches`. On peut descendre à ~20 ms en refusant les
  notes sous 100 Hz ; c'est un choix, pas une limite technique cachée.
- **Rattrapage du vibrato** : le moteur suit le vibrato mais ne le « préserve »
  pas encore comme le fait Waves (contrôle dédié). Les réglages
  tolerance cents / temps ne sont pas ajoutés : ils n'ont de sens qu'avec un
  moteur à faible latence, ce qui est maintenant le cas — c'est le prochain
  chantier logique.

## Installation

Fermer FL Studio, remplacer `Alpha-Tune.vst3` dans
`C:\Program Files\Common Files\VST3\`, supprimer `%TEMP%\hardtune-ui-*.txt`,
puis rescanner les plugins.

## Vérifications

- 12 tests hors DAW : **12/12**.
- ASan + UBSan : aucune erreur, aucune fuite.
- Voix réelle (`voix-brute.wav`, 21,46 s) : voir le tableau ci-dessus.
- Build Windows (workflow GitHub) : voir le run associé au commit.
