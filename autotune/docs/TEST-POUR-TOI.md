# Ton chemin exact — de zéro au premier son

**D'après ton diagnostic :** Windows 11 64 bits ✅, CMake ✅, dossier VST3 ✅,
FL Studio ✅, Cubase ✅. Il te manque **Git** et **le compilateur C++**.

Deux chemins possibles. **Je te recommande le B** : rien à installer, et ça sert aussi
à publier le projet open source plus tard.

---

# CHEMIN B (recommandé) — GitHub compile à ta place

**Ce qu'il te faut : un compte GitHub (gratuit). Rien à installer sur ton PC.**
**Durée : 15 min, dont 10 min d'attente pendant que ça compile.**

## B1. Créer le compte (si tu n'en as pas) — 2 min

1. Va sur <https://github.com/signup>
2. Email, mot de passe, nom d'utilisateur. Valide l'email.

## B2. Créer le dépôt — 1 min

1. Va sur <https://github.com/new>
2. **Repository name** : `hardtune`
3. Coche **Public** *(obligatoire pour utiliser les minutes gratuites sans limite)*
4. Ne coche **rien d'autre** (pas de README, pas de .gitignore)
5. **Create repository**

## B3. Envoyer les fichiers — 3 min

1. Sur la page du dépôt vide, clique sur le lien **« uploading an existing file »**
   *(ou : Add file → Upload files)*
2. Ouvre dans l'explorateur le dossier **`autotune`** que tu viens de décompresser.
3. Sélectionne **tout ce qu'il y a À L'INTÉRIEUR** (Ctrl+A) — donc `Source`, `docs`,
   `outils`, `tests`, `.github`, `CMakeLists.txt`, `build.bat`, `README.md`, etc.
   ⚠️ **Pas le dossier `autotune` lui-même**, son contenu seulement.
4. Glisse-dépose dans la page GitHub.
5. En bas, clique sur **Commit changes**.

> Vérifie après l'envoi que tu vois bien ces dossiers à la racine du dépôt :
> `Source/`, `docs/`, `outils/`, `tests/`, `.github/`, et le fichier `CMakeLists.txt`.
> S'il manque `.github`, refais le glisser-déposer en affichant les éléments cachés de
> l'explorateur (onglet *Affichage* → cocher *Éléments masqués*).

## B4. Laisser GitHub compiler — 10 min, tu ne fais rien

1. Onglet **Actions** en haut du dépôt.
2. Une ligne « Build » doit apparaître avec un point jaune (en cours).
3. Attends qu'elle passe au **coche verte** ✅ (10 à 15 min la première fois).

Si tu vois une **croix rouge** ❌ : clique dessus, ouvre le job `Windows VST3`,
copie la partie rouge du log et envoie-la moi.

## B5. Télécharger le plugin compilé — 1 min

1. Toujours dans **Actions**, clique sur le build terminé (coche verte).
2. En bas de la page, section **Artifacts** :
3. Clique sur **`HardTune-VST3-Windows`** → un `.zip` se télécharge.
4. Décompresse-le. Tu obtiens :
   - `HardTune.vst3` → **un dossier** (pas un fichier unique, c'est normal sous JUCE)
   - `HardTune.exe` → la version autonome (bonus, pour tester sans DAW)

## B6. Installer le plugin — 1 min

Copie le **dossier** `HardTune.vst3` dans :

```
C:\Program Files\Common Files\VST3
```

Ce dossier demande les droits administrateur. Le plus simple :

1. Menu Démarrer → tape `cmd` → clic droit → **Exécuter en tant qu'administrateur**
2. Colle (adapte le chemin si tu as téléchargé ailleurs) :

```bat
xcopy /E /I "%USERPROFILE%\Downloads\HardTune.vst3" "C:\Program Files\Common Files\VST3\HardTune.vst3"
```

3. Vérifie : `dir "C:\Program Files\Common Files\VST3\HardTune.vst3"` doit lister
   un dossier `Contents`.

---

# CHEMIN A (alternative) — compiler sur ton PC

À choisir **seulement** si tu ne veux pas de compte GitHub.

1. Installer **Git** : <https://git-scm.com/download/win> (options par défaut, 60 Mo)
2. Installer **Visual Studio Build Tools 2022** (pas le VS complet, ~2 Go) :
   <https://visualstudio.microsoft.com/downloads/> → *Outils de génération pour Visual
   Studio 2022* → dans l'installateur, cocher **« Développement Desktop en C++ »**
3. Dans le dossier `autotune`, double-cliquer `build.bat`.
4. Suivre ensuite l'étape **B6** ci-dessus.

---

# ENSUITE : le test dans FL Studio (10 min)

*(Variante Cubase à la fin.)*

## Étape 1 — Faire scanner le plugin

1. Ouvre FL Studio.
2. Menu **Options → Manage plugins** (ou l'icône de prise en bas de la *Browser*).
3. En haut à gauche : **Find more plugins** → **Start scan**.
4. Dans la liste, cherche **HardTune** (dossier *Installed → VST3*).

> S'il n'apparaît pas : vérifie que le chemin `C:\Program Files\Common Files\VST3`
> est bien coché dans les dossiers scannés par FL, puis relance le scan.

## Étape 2 — L'insérer sur une piste voix

1. Charge un **enregistrement de voix chantée** sur une piste (drag & drop dans la playlist).
2. Ouvre la **Mixer** (F9), sélectionne la piste où va la voix.
3. Dans un slot d'effet, clique → **HardTune**.
4. La fenêtre du plugin s'ouvre.

## Étape 3 — Les 6 vérifications

| # | Tu fais | Tu vérifies |
|---|---|---|
| **1** | Lecture de la voix | La voix s'entend, aucun craquement. L'interface s'affiche proprement (rien de coupé / superposé) |
| **2** | Regarde l'écran à droite | La note affichée correspond à ce qui est chanté ; le Hz se stabilise sur les notes tenues |
| **3** | Clique **T-PAIN** (bouton rouge) | La voix se cale sur les notes — effet robotique mais juste |
| **4** | Tourne **RETUNE** de 0 → 300 ms | L'effet passe de « robot instantané » à « correction lente et naturelle » |
| **5** | Tourne **FORMANT** de 0 → +60 puis −60 | La voix devient plus fine puis plus grave, **sans changer de note** |
| **6** | Change la **tonalité** (roue) et la **gamme** | Le résultat change ; mets la tonalité réelle de ton morceau |

**Bonus robustesse (si tu as 2 min) :** change le sample rate du projet FL
(*Options → Audio settings*), sauvegarde/rouvre, mets 3 instances.

---

## Variante Cubase (si tu préfères)

1. **Studio → VST Plug-in Manager** → onglet *VST 3* → **Update** (ou redémarrer Cubase)
2. Ajoute une piste Audio, insère **HardTune** dans les inserts.
3. Mêmes 6 vérifications.

---

# Ce que tu m'envoies (copie-colle)

```
A. Le build GitHub :        réussi / erreur (coller le log rouge)
B. Le plugin apparaît dans FL Studio :   oui / non
C. 1. Ça s'ouvre et le son passe :       oui / non
   2. Notes détectées :                   justes / une octave à côté / n'importe quoi
   3. Son T-PAIN :                        ça y est / presque / pas du tout
   4. RETUNE :                            fonctionne / aucun effet
   5. FORMANT :                           utile / métallique / inaudible
   6. Gamme + tonalité :                  fonctionnent / sans effet
D. Artefacts :              aucun / clics / grains / chevrotement / double voix
E. Interface :              propre / à corriger → + capture d'écran
```

**Le point E et le point 3 sont les plus importants** : ce sont les deux choses que je n'ai
pas pu vérifier de mon côté (je n'ai jamais vu l'interface ni entendu le son).
