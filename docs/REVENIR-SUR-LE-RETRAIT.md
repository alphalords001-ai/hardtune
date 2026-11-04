# Revenir sur le retrait du commutateur PITCH CORRECTION

> Retrait effectué en **v0.8.1**. Version précédente conservée : commit **`3e72451`**
> (tag local `v0.8.0-avec-commutateur`).

## Ce qui a été retiré, exactement

| élément | état v0.8.0 | état v0.8.1 |
|---|---|---|
| commutateur dans l'interface | présent | **supprimé** |
| témoin ON / OFF | présent | **supprimé** |
| paramètre `correctionOn` | présent | **supprimé** |
| accord | dosé par AMOUNT × commutateur | **dosé par AMOUNT seul** |

## Pourquoi

Le commutateur ne coupait **que l'accord**, jamais les effets. Mesuré sur une
sinusoïde à 226 Hz (cible A3 = 220 Hz), commutateur sur OFF :

| état | sortie | ce qui se passait |
|---|---|---|
| OFF, sans vibrato | 226,00 Hz fixe | rien, correct |
| OFF, VIBRATO 50 % | 222,86 → 229,18 Hz | **48 cents de vibrato ajoutés** |
| OFF, HUMANIZE 100 % | 225,06 → 227,76 Hz | **21 cents de dérive ajoutés** |

Sur OFF, le plugin continuait donc de modifier la voix. Le commutateur ne
« coupait » rien de façon fiable, et c'est ce qui rendait son effet impossible
à interpréter à l'écoute.

## Comment revenir en arrière

**Option 1 — annuler le commit de retrait (le plus sûr)**

```bash
git revert 2b05629
```

**Option 2 — réappliquer le commutateur sur l'état v0.8.1**

```bash
git apply docs/retour-commutateur-pitch-correction.patch
```

Vérifié : ce patch s'applique proprement sur l'état v0.8.1. Si le code a
évolué depuis et qu'il refuse, essayer `git apply -3` ; en cas de conflit,
l'option 3 reste toujours valable.

**Option 3 — repartir de l'état complet d'avant**

```bash
git checkout 3e72451        # ou : git checkout v0.8.0-avec-commutateur
```

## Ce qu'il faut savoir si on restaure

- Le paramètre `correctionOn` **revient**, avec son identifiant d'origine : les
  presets d'avant v0.8.1 le retrouveront.
- Les presets enregistrés **entre-temps** (en v0.8.1) ne contiennent pas ce
  paramètre : il reprendra sa valeur par défaut. Le code lit les paramètres de
  façon tolérante (`raw()` renvoie la valeur par défaut si l'identifiant est
  absent), donc **pas de plantage** — mais le commutateur démarrera sur sa
  valeur par défaut, pas sur celle qu'on aurait voulu.
- Après restauration, relancer les tests hors DAW et refaire la mesure OFF/ON,
  car le défaut d'origine (vibrato et humanize actifs sur OFF) **revient avec
  le commutateur**. Le retrait ne corrige pas ce défaut : il le supprime en
  supprimant le commutateur.

## Le fond du problème, indépendant de ce commutateur

État au **v0.8.2**. Trois couplages existaient ; deux sont corrigés, le
troisième est un choix assumé.

| couplage | v0.8.1 | v0.8.2 |
|---|---|---|
| `TRANSPOSE` multiplié par `AMOUNT` (à 0 %, il ne faisait rien) | présent | **corrigé** |
| `RETUNE` qui étouffait `VIBRATO` et `HUMANIZE` (48,4 ¢ → 3,5 ¢) | présent | **corrigé** |
| `VIBRATO` et `HUMANIZE` non dosés par `AMOUNT` | présent | **assumé** — ce sont des effets, pas des erreurs de justesse |

`AMOUNT` ne dose donc plus que l'accord. Chaque commande de l'interface a
maintenant un effet isolé et prévisible.
