# Compiler avec GitHub — mode d'emploi exact

**Budget data estimé : 12 à 18 Mo au total.** (À comparer aux 2,5 Go de Visual Studio.)
**Durée : 20 minutes, dont 12 à 15 d'attente sans rien faire.**

Le principe : GitHub compile sur ses propres serveurs Windows (gratuit, illimité pour un
dépôt public). Tu n'envoies que le code (75 Ko) et tu récupères le plugin compilé (~4 Mo).

---

## ÉTAPE 1 — Télécharger et décompresser le projet

1. Télécharge **`autotune-v0.3.zip`** (75 Ko).
2. Clic droit → **Extraire tout** → tu obtiens un dossier `autotune`.

> Ne modifie rien dedans, et n'ouvre aucun fichier : tout se passe sur le site GitHub.

**✔ Checkpoint :** le dossier contient `Source`, `docs`, `outils`, `tests`, `.github`,
`CMakeLists.txt`, `README.md`.

> ⚠️ Si tu ne vois pas le dossier **`.github`** : dans l'explorateur, onglet **Affichage** →
> coche **Éléments masqués**. Il est indispensable (c'est lui qui déclenche la compilation).

---

## ÉTAPE 2 — Créer le dépôt (2 min)

1. <https://github.com> → **Sign up** (si tu n'as pas de compte). Email + mot de passe,
   valide l'email. *Aucune carte bancaire demandée.*
2. Une fois connecté : <https://github.com/new>
3. **Repository name** : `hardtune`
4. Sélectionne **Public** *(obligatoire : les minutes de compilation sont illimitées
   uniquement en public)*
5. **Ne coche RIEN d'autre** (ni Add a README, ni .gitignore, ni license)
6. **Create repository**

**✔ Checkpoint :** tu arrives sur une page « Quick setup » avec un lien
*« uploading an existing file »*.

---

## ÉTAPE 3 — Envoyer le code (3 min, 75 Ko)

1. Clique sur le lien **« uploading an existing file »**
   *(ou bouton **Add file ▾** → **Upload files**)*
2. Dans l'explorateur, **ouvre le dossier `autotune`** et fais **Ctrl+A**
   (tu sélectionnes son *contenu*, pas le dossier lui-même).
3. **Glisse-dépose** dans la zone de la page GitHub.
4. Attends que tous les fichiers apparaissent dans la liste (30 fichiers).
5. En bas, clique sur le bouton vert **Commit changes**.

**✔ Checkpoint — le plus important :** sur la page du dépôt, à la racine, tu dois voir :

```
Source/    docs/    outils/    tests/    .github/    CMakeLists.txt    README.md
```

❌ Si tu vois un dossier `autotune/` **au lieu de** ces éléments → tu as envoyé le dossier
au lieu de son contenu. Supprime-le et recommence l'étape 3.

❌ Si `.github` n'apparaît pas → il n'a pas été envoyé. Passe à l'**étape 3 bis**.

### Étape 3 bis — ajouter `.github` à la main (si absent)

1. Dans le dépôt : **Add file ▾** → **Create new file**
2. Dans le champ du nom, tape exactement : `.github/workflows/build.yml`
   *(les `/` créent les dossiers automatiquement)*
3. Colle ce contenu :

```yaml
name: Build

on:
  push:
  workflow_dispatch:

jobs:
  windows:
    name: Windows VST3
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Configurer
        run: cmake -B build -G "Visual Studio 17 2022" -A x64
      - name: Compiler
        run: cmake --build build --config Release --parallel
      - name: Publier le .vst3
        uses: actions/upload-artifact@v4
        with:
          name: HardTune-VST3-Windows
          path: build/**/VST3/*.vst3
          if-no-files-found: error
```

4. **Commit changes**

---

## ÉTAPE 4 — Laisser compiler (12 à 15 min, tu ne fais rien)

1. Onglet **Actions** en haut du dépôt.
2. Une ligne **« Build »** apparaît avec un point **jaune** (en cours).
3. Attends qu'elle passe en **coche verte** ✅.

> Le point jaune peut rester 2-3 minutes avant de bouger : c'est normal, GitHub prépare
> la machine Windows.

**✔ Checkpoint :** coche verte + le mot « success ».
**❌ Si croix rouge :** clique sur « Build », puis sur le job **Windows VST3** →
ouvre l'étape **Compiler** → copie les 20 dernières lignes → envoie-les moi.
*Ça ne te coûte presque rien : c'est du texte.*

---

## ÉTAPE 5 — Télécharger le plugin (1 min, ~4 Mo)

1. Toujours dans **Actions**, clique sur le build terminé.
2. Descends en bas de la page : section **Artifacts**.
3. Clique sur **`HardTune-VST3-Windows`**.
4. Un fichier `.zip` se télécharge (3 à 6 Mo).
5. Décompresse-le : tu obtiens **`HardTune.vst3`** — c'est un **dossier**, c'est normal.

**✔ Checkpoint :** dans le dossier `HardTune.vst3`, tu vois un dossier `Contents`.

---

## ÉTAPE 6 — Installer (1 min)

1. Menu Démarrer → tape `cmd` → clic droit → **Exécuter en tant qu'administrateur**
2. Colle cette commande *(remplace `TON_NOM` par ton nom d'utilisateur)* :

```bat
xcopy /E /I "C:\Users\TON_NOM\Downloads\HardTune.vst3" "C:\Program Files\Common Files\VST3\HardTune.vst3"
```

3. Vérifie :

```bat
dir "C:\Program Files\Common Files\VST3\HardTune.vst3\Contents"
```

**✔ Checkpoint :** tu vois `x86_64-win`.

---

## ÉTAPE 7 — Tester dans FL Studio (10 min)

1. Ouvre FL Studio.
2. **Options → Manage plugins** → **Find more plugins** → **Start scan**
3. Cherche **HardTune** dans la liste.
4. Sur une piste voix : ouvre la Mixer (F9) → clic sur un slot → **HardTune**.
5. Clique **T-PAIN**, lance la lecture.

### Les 6 vérifications

| # | Tu fais | Attendu |
|---|---|---|
| 1 | Regarde la fenêtre | Interface affichée proprement, rien de coupé |
| 2 | Chante ou lance une voix | La note détectée correspond, l'aiguille bouge |
| 3 | Preset T-PAIN | La voix se cale sur les notes, effet assumé |
| 4 | RETUNE 0 → 300 ms | L'effet devient plus lent, plus naturel |
| 5 | FORMANT 0 → +60 / −60 | Timbre plus fin puis plus grave, **sans changer la note** |
| 6 | Change tonalité + gamme | Le résultat change clairement |

---

## Ce que tu m'envoies

```
Build GitHub :        vert / rouge (+ log)
Plugin trouvé :       oui / non
1. Interface :        propre / à corriger (+ capture)
2. Détection :        justes / une octave à côté / n'importe quoi
3. Son T-PAIN :       ça y est / presque / pas du tout
4. RETUNE :           fonctionne / sans effet
5. FORMANT :          utile / métallique / rien
6. Gamme :            fonctionne / sans effet
```

---

## Si tu veux économiser encore

- **Ne réessaie pas 5 builds** : chaque build coûte ~4 Mo de téléchargement. Si ça échoue,
  envoie-moi le log (du texte, quelques Ko) et j corrige d'abord.
- La **démo navigateur** (`demo/hardtune-demo.html`, 29 Ko) reste utilisable sans rien
  compiler : elle fait déjà entendre le moteur.
- Évite de recharger les pages GitHub en boucle (chaque page ≈ 1-2 Mo).

**Coût réel attendu : 12 à 18 Mo.** Il te restera de la marge.
