# Correctif v0.8.0 — moteur Live à 2,7 ms

## Ce qui n'allait pas

Le moteur TD-PSOLA de la v0.7.0 programmait ses grains jusqu'à ce que
`nextCenter + period` dépasse la sortie demandée. Le centre du **dernier** grain
tombait donc une période en arrière, et la moitié de sa fenêtre de Hann
s'écrivait sur des échantillons **déjà lus puis remis à zéro**. La correction
n'arrivait qu'au tour suivant de l'anneau de sortie.

Mesuré sur une sinusoïde de 220 Hz corrigée vers 233,3 Hz :

| anneau de sortie | retard de réponse du moteur |
|---|---|
| ×1 (4096 éch.) | **72 ms** |
| ×2 (8192 éch.) | 165 ms |
| ×4 (16384 éch.) | 350 ms |

Le retard ne dépendait ni de la note ni du rapport de correction : il suivait la
taille de l'anneau. C'est cette marche — la sortie reste à l'ancienne hauteur
puis saute d'un demi-ton d'un coup — qui s'entendait comme « robotique ».

## Ce qui remplace PSOLA

Une **ligne à retard à lecture pitch-synchrone** : le passé est lu à la vitesse
`ratio = 2^(cents/1200)`. Quand le point de lecture a dérivé d'un cycle, on saute
exactement une période — en arrière si on a rattrapé l'écriture, en avant si on a
traîné. La longueur du saut est trouvée par corrélation croisée au point de
lecture, avec raffinement sous-échantillon, et le raccord est fondu sur un
cosinus surélevé renormalisé en puissance selon la corrélation mesurée.

Il n'y a plus aucun tampon de grains : la correction s'applique à l'échantillon
près.

## Résultats mesurés

| | v0.7.0 (PSOLA) | v0.8.0 (Live) |
|---|---|---|
| Latence annoncée | 1152 éch. (26,1 ms) | **119 éch. (2,7 ms)** |
| Réponse à un changement de note | 72 ms | **15,8 ms** |
| CPU (voix réelle, bloc 512) | 2,1 % d'un cœur | **0,5 %** |
| Justesse, écart médian | 7,9 cents | 11,3 cents |
| Justesse, p90 | 38,6 cents | 40,7 cents |
| Sauts mesures > 50 cents | 365 | 465 |
| Tests hors DAW | 12/12 | **13/13** |

**La justesse se dégrade de 3,4 cents.** Ce n'est pas caché : le moteur PSOLA
resynthétise un signal propre, alors que la lecture à vitesse non entière laisse
passer les transitoires des raccords. Le banc mesure la hauteur par
autocorrélation sur des fenêtres de 23 ms, donc chaque saut de cycle y apparaît
comme une erreur. Les notes restent justes (< 50 cents partout), mais la mesure
est plus bruitée.

Trois pistes ont été testées pour récupérer cet écart, **aucune n'a marché** :

| essai | résultat |
|---|---|
| Interpolation cubique (Catmull-Rom) à la lecture | 11,3 → 11,3 cents : aucun effet |
| Fenêtre d'analyse 3 / 4 / 6 périodes ou maximale | 11,8 / 12,6 / 14,5 / 15,2 : pire |
| Retard de commande 4 / 8 / 12 / 16 ms | 13,3 / 16,2 / 17,8 / 18,4 : pire |

Le zéro retard de commande est donc le bon choix, et 2,2 périodes de fenêtre le
meilleur compromis.

## Ce qui change dans l'interface

- Latence affichée et annoncée au DAW : 2,7 ms.
- **FORMANT est inactif.** Un moteur à ligne à retard ne peut pas déplacer les
  formants indépendamment de la hauteur : ils suivent la note. Le paramètre est
  conservé pour ne pas invalider les presets, mais son contrôle est grisé et
  affiche « — ».

## Tests ajoutés

- **TEST 12 — sauts de cycle sans clic** : sur une sinusoïde transposée d'un
  demi-ton, la plus grande différence entre deux échantillons consécutifs de la
  sortie ne dépasse pas celle de la sinusoïde elle-même (mesuré : rapport 1,00,
  0 saut).
- **TEST 13 — réponse immédiate** : mesure par passages par zéro interpolés ;
  la sortie atteint la nouvelle hauteur **15,8 ms** après le changement (72 ms
  en v0.7.0). Une fenêtre de Goertzel de 512 échantillons ne suffit pas à ce test
  (94 Hz de résolution, 220 et 233 Hz tombent dans le même lobe).
- Le test de préservation des formants est supprimé : il vérifiait une propriété
  que ce moteur n'a plus, par construction.

## Compilation des tests

```
g++ -O2 -std=c++17 -I Source/dsp tests/dsp_offline.cpp \
    Source/dsp/PitchDetector.cpp Source/dsp/LiveEngine.cpp \
    Source/dsp/TuneCore.cpp -o dsp_tests
```
