# v0.5.1 — deux corruptions mémoire corrigées + interface compacte

Cette version corrige les causes probables du **DAW qui se ferme tout seul**, et
réduit la fenêtre de **960 × 1050** à **820 × 600**.

---

## 1. Pourquoi le DAW se fermait

Deux défauts distincts écrivaient en dehors des tampons alloués. Trouvés en
rejouant le moteur hors DAW avec **AddressSanitizer**, sur un ordinateur qui
simule les comportements d'hôte « normaux mais pas prévus » :

| # | Défaut | Déclencheur | Effet |
|---|---|---|---|
| 1 | `TuneCore::process` écrivait `dryDelay` avec **plus de canaux** que `prepare()` n'en avait vu | instance préparée en mono, puis l'hôte envoie 2 canaux (changement de bus, wrapper de DAW) | `heap-buffer-overflow`, tas corrompu → DAW fermé |
| 2 | `retrieve()` de Rubber Band recevait un **tableau de pointeurs partiellement non initialisé** | moteur créé en stéréo, l'hôte envoie un bloc mono | `retrieve` écrit à travers un pointeur au hasard → plantage immédiat |

Preuve, avant correction :

```
ERROR: AddressSanitizer: heap-buffer-overflow
WRITE of size 4 ... in tune::TuneCore::process  Source/dsp/TuneCore.cpp:238
freed by ...                            (cas « 1 canal préparé, 2 reçus »)
ERROR: AddressSanitizer: heap-use-after-free
#3 RubberBand::R3Stretcher::retrieve   (cas « 2 canaux préparés, 1 reçu »)
```

Après correction, les six combinaisons hostiles passent sous
AddressSanitizer **et** UndefinedBehaviorSanitizer : bloc plus grand que
`prepare()`, bloc de 4096 samples après un `prepare()` à 64, mono→stéréo,
stéréo→mono, taille de bloc non multiple.

## 2. Ce qui a changé dans le code

**`Source/dsp/TuneCore.cpp`**
- `process()` borne désormais les canaux à ce que `prepare()` a réellement
  alloué (`used = min(canaux_hôte, canaux_préparés)`), puis duplique le dernier
  canal traité sur les canaux restants.
- `process()` découpe le bloc en tranches de `maxBlock` : un hôte qui envoie
  4096 samples après un `prepare()` à 512 ne déborde plus de `tmpWet`.
- `retrieve()` reçoit exactement `numChannels` pointeurs valides (tableau
  initialisé à `nullptr`), plus jamais de pointeur indéterminé.
- Le corps du traitement est passé dans `processChunk(in, out, offset, n, canaux)`.

**`Source/PluginProcessor.cpp`**
- `prepareToPlay`, `processBlock` et `createEditor` sont entourés de `try/catch` :
  une exception ne traverse plus l'interface VST3 (sur Windows, c'est une
  fermeture immédiate du DAW). En cas d'échec, le plugin reste chargé et
  transparent.
- `processBlock` refuse un tampon sans canal (`getNumChannels() <= 0`) au lieu
  d'appeler `getReadPointer(0)`.
- `syncEngineFromParameters` lit tous les paramètres via un accesseur tolérant :
  un identifiant absent (état d'une ancienne version) ne provoque plus de
  déréférencement nul.

## 3. Interface : 820 × 600 au lieu de 960 × 1050

- 1050 px de haut ne tenaient sur **aucun** écran 1080p (barre des tâches +
  barre de titre) : la fenêtre était rognée en bas.
- La nouvelle fenêtre fait **49 % de l'aire** de l'ancienne et tient sur un
  portable 1366 × 768.
- Ce n'est pas un simple zoom : la page est redistribuée (en-tête 44 px,
  clavier 76 px, colonne de droite 292 px, grand cadran Ø182), les tailles de
  texte ont été ajustées pour rester lisibles, et les deux barres
  FORMANT/TRANSPOSE ont maintenant la place pour leur poignée de 22 px.
- Repères : la plupart des plugins audio tournent entre **600 × 400** et
  **900 × 600** (les plus compacts, type Valhalla/Waves, autour de 500 × 350 ;
  les plus larges, type Serum, autour de 900 × 600).

## 4. Ce qui n'a pas changé

- Le DSP est identique : les 10 séries de tests passent toujours
  (`TOUS LES TESTS PASSENT (0 échec(s))`), y compris latence, justesse,
  transparence à `amount = 0`, mono qui reste mono, humanize et vu-mètres.

## 5. À vérifier sur Windows

1. Le plugin se charge-t-il sans fermer FL Studio ?
2. La fenêtre fait-elle bien 820 × 600 et tient-elle entièrement à l'écran ?
3. Si ça plante encore : Observateur d'événements → Journaux Windows →
   Application, et relève le **module fautif** et le **code d'exception**
   (`0xc0000005` = pointeur, `0xe06d7363` = exception C++).
