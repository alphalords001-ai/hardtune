# Protocole de test — première écoute

**Durée : 10 à 15 minutes. Objectif : savoir si le plugin fonctionne, et surtout s'il sonne.**

Matériel : ton PC Windows, ton DAW, une piste voix (un enregistrement de chant suffit —
pas besoin de chanter en direct pour ce premier test).

---

## Préparation (2 min)

1. Ouvre ton DAW.
2. Crée une piste audio, glisse dessus **un enregistrement de voix chantée** (10–30 s,
   avec quelques notes tenues et si possible un peu de parole).
3. Insère **HardTune** sur cette piste.
4. Réglez le **buffer audio** du DAW sur 256 ou 512 samples (pas 1024) pour que la latence
   ne te gêne pas si tu testes au micro.

---

## Test 1 — Il s'ouvre et il ne casse rien (1 min)

**Tu fais :** tu lances la lecture de la voix.

**Tu vérifies :**
- [ ] La fenêtre du plugin s'affiche correctement (rien de coupé, pas de texte qui se chevauche)
- [ ] Tu entends la voix
- [ ] Aucun craquement, aucun silence

> Si l'interface est cassée visuellement (éléments qui se chevauchent, textes illisibles) :
> **prends une capture d'écran**, c'est le point que je n'ai pas pu vérifier.

---

## Test 2 — Il détecte la bonne note (2 min)

**Tu fais :** tu cliques sur **T-PAIN**, tu lances la lecture, et tu regardes l'écran à droite.

**Tu vérifies :**
- [ ] Le gros caractère affiche les notes qui **correspondent à ce qui est chanté**
- [ ] Quand une note est tenue, le chiffre en Hz se stabilise (ex. une voix féminine
      autour de 220–440 Hz, une voix masculine 100–250 Hz)
- [ ] L'aiguille de cents se place des deux côtés du centre selon la justesse
- [ ] La petite LED en haut à droite s'allume quand il y a de la voix, s'éteint dans les silences

> C'est le **verrou n°1**. Si les notes affichées sont fausses (une octave à côté,
> n'importe quoi sur les consonnes), dis-le moi : c'est le DSP qu'il faut revoir, pas la suite.

---

## Test 3 — Le son « T-Pain » (3 min) ⭐ LE TEST LE PLUS IMPORTANT

**Tu fais :** preset T-PAIN (bouton rouge), lecture de la voix.

**Tu écoutes :**
- [ ] La voix est-elle **calée sur les notes** ? (effet robotique mais juste)
- [ ] Le timbre est-il **proche de la voix d'origine** ? (pas une voix de dessin animé)
- [ ] Y a-t-il des **artefacts bizarres** : clics, grains, « chevrotement », double voix ?

**Puis tu tournes les boutons un par un, en écoutant :**

| Bouton | Ce que tu dois constater |
|---|---|
| **RETUNE** de 0 → 300 ms | l'effet passe de « robot instantané » à « correction naturelle et lente ». À 300 ms, on doit entendre la voix glisser vers la note. |
| **AMOUNT** de 100 → 0 % | à 0 %, le traitement doit devenir **transparent** (voix d'origine, juste un peu retardée) |
| **FORMANT** de 0 → +60 | la voix doit devenir plus **fine / nasillarde**, **sans changer de note** |
| **FORMANT** de 0 → −60 | la voix doit devenir plus **grave / épaisse**, sans changer de note |
| **MIX** de 100 → 50 % | tu dois entendre la voix brute se mélanger à la voix corrigée |
| **BYPASS** | retour à la voix d'origine, sans clic |

> C'est le **verrou n°2** : celui où tu me dis « ça y est » ou « ce n'est pas ça ».
> Si le formant sonne métallique ou si tu entends des clics, c'est normal que je doive
> retoucher — c'est le réglage le plus délicat du moteur.

---

## Test 4 — Gamme et tonalité (2 min)

**Tu fais :**
1. Choisis la **tonalité réelle du morceau** (bouton de la roue, ex. « A »).
2. Choisis la **gamme** (ex. « Minor »).
3. Mets RETUNE à 0 et AMOUNT à 100.
4. Écoute.

**Tu vérifies :**
- [ ] La voix se cale sur les notes de la gamme choisie
- [ ] En changeant la tonalité (A → C), le résultat change clairement
- [ ] En « Chromatic », toutes les notes passent (correction vers le demi-ton le plus proche)

---

## Test 5 — Robustesse (2 min)

**Tu vérifies :**
- [ ] Change le **sample rate** du projet (44,1 → 48 kHz) : pas de crash, la voix est toujours juste
- [ ] Change la **taille de buffer** (128 → 1024) : pas de clic, résultat identique
- [ ] **Sauvegarde le projet, ferme, rouvre** : les réglages sont là
- [ ] Monte **3 instances** du plugin sur 3 pistes : pas de plantage
- [ ] Regarde le **CPU** du DAW : doit rester sous ~5 % par instance

---

## Test 6 — Ce que tu n'as PAS à tester

Inutile de te pencher là-dessus pour l'instant :
- ❌ Ubuntu / Linux (étape 16, plus tard)
- ❌ L'installeur `.exe` (étape 15)
- ❌ La compatibilité avec 10 DAW différents (le tien suffit)
- ❌ Le rendu sur 5 styles de voix différents (une prise suffit pour un premier verdict)

---

## Comment me répondre (copie-colle ça)

```
1. Ça s'ouvre ?            OUI / NON (+ capture si l'interface est cassée)
2. Notes détectées ?       justes / une octave à côté / n'importe quoi
3. Son T-PAIN ?            ça y est / presque / pas du tout
   - détail : ...
4. Artefacts ?             aucun / clics / grains / chevrotement / double voix
5. Formant ?               utile / métallique / inaudible
6. Retune / Amount ?       fonctionnent / l'un des deux ne fait rien
7. Gamme / tonalité ?      fonctionnent / pas d'effet
8. Robustesse ?            tout ok / (préciser lequel a échoué)
9. Interface :             propre / à corriger (préciser) + capture d'écran
```

Avec ça, je sais exactement quoi retoucher. Le point 3 est celui qui décide de la suite :
si le son te plaît, on passe à l'interface et au packaging ; sinon, on retravaille le moteur.
