# Compiler HardTune sous Windows — pas à pas

**Temps total : 30 à 45 minutes** (dont ~15 min d'installation, ~10 min de compilation).
Tu n'as rien à programmer : tu installes trois outils, tu lances un script.

---

## Étape 1 — Visual Studio 2022 Community (le compilateur)

1. Télécharge : <https://visualstudio.microsoft.com/fr/vs/community/>
2. Lance l'installateur.
3. Sur l'écran **« Charges de travail »**, coche **une seule case** :

   > ☑ **Développement Desktop en C++**  *(Desktop development with C++)*

   Vérifie dans le panneau de droite que **« MSVC v143 »** et **« Windows 11 SDK »**
   sont bien cochés (ils le sont par défaut avec cette charge de travail).
4. Clique **Installer** (~6 Go, 15–20 min).
5. À la fin : **redémarre** si l'installateur le demande.

> ⚠️ Ne coche rien d'autre (pas besoin de .NET, UWP, Unity…).

---

## Étape 2 — CMake et Git

**CMake** (le chef d'orchestre du build)
1. <https://cmake.org/download/> → *Windows x64 Installer*
2. Pendant l'installation, **choisis « Add CMake to the system PATH »**
   (l'option « for all users » est le plus simple).

**Git** (sert à télécharger JUCE automatiquement)
1. <https://git-scm.com/download/win>
2. Installation en laissant les options par défaut.

**Vérification** : ouvre le *Menu Démarrer* → tape `cmd` → Entrée, puis tape :

```bat
cmake --version
git --version
```

Tu dois voir deux numéros de version. Si « commande introuvable » → l'option PATH
n'a pas été cochée, désinstalle/réinstalle CMake en cochant l'option.

---

## Étape 3 — Récupérer le projet

Deux possibilités :

- **A.** Tu as le dossier `autotune` : copie-le où tu veux, par exemple
  `C:\Dev\autotune` (évite les chemins avec accents ou espaces).
- **B.** Depuis un dépôt Git (plus tard, quand on aura publié) :
  ```bat
  git clone <url-du-depot> C:\Dev\autotune
  ```

---

## Étape 4 — Compiler

1. Ouvre le dossier `C:\Dev\autotune`.
2. **Double-clique sur `build.bat`.**

Le script :
- configure le projet avec Visual Studio 2022,
- télécharge JUCE (la première fois uniquement, ~150 Mo),
- compile le plugin en mode Release,
- affiche le chemin du `.vst3` produit.

Compte **10 à 15 minutes** au premier build. Les suivants prennent 1 à 2 minutes.

> Si une fenêtre noire se ferme trop vite : lance `build.bat` depuis
> *cmd* en te plaçant dans le dossier, tu verras le message d'erreur complet.

---

## Étape 5 — Installer le plugin

Le fichier produit ressemble à :

```
C:\Dev\autotune\build\HardTune_artefacts\Release\VST3\HardTune.vst3
```

Copie ce **dossier** `HardTune.vst3` dans :

```
C:\Program Files\Common Files\VST3
```

Ce dossier demande les droits administrateur : soit tu cliques-droit → *Copier* puis
dans le dossier cible → clic droit → *Coller avec les droits admin*, soit tu copies
depuis une invite `cmd` **lancée en administrateur** :

```bat
xcopy /E /I "C:\Dev\autotune\build\HardTune_artefacts\Release\VST3\HardTune.vst3" "C:\Program Files\Common Files\VST3\HardTune.vst3"
```

> `build.bat` essaie aussi de le faire automatiquement (option `COPY_PLUGIN_AFTER_BUILD`),
> mais Windows refuse souvent la copie sans droits admin : la copie manuelle est normale.

---

## Étape 6 — Premier test dans ton DAW

1. Ouvre ton DAW, **relance le scan des plugins** (ou redémarre-le).
2. Sur une piste voix (micro ou fichier audio), insère **HardTune** dans la chaîne d'effets.
3. Clique sur **T-PAIN** en haut à droite de l'interface.
4. Chante ou lance la voix : tu dois voir la note détectée s'afficher et la voix se caler
   sur la gamme.

**Les 4 choses à vérifier :**

| Test | Ce que tu dois voir / entendre |
|---|---|
| Note détectée | Le gros caractère à droite affiche la bonne note quand tu chantes |
| Aiguille | Elle se stabilise au centre quand la note est juste |
| Effet | Avec le preset T-PAIN, la voix est calée, effet robotique assumé |
| Retune | Remonte le bouton RETUNE vers 300 ms → l'effet devient plus naturel/plus lent |

---

## Si ça ne compile pas

Copie-colle moi **le message d'erreur complet** (les 15 dernières lignes suffisent) et
précise à quelle étape ça bloque. Les causes habituelles :

| Message | Cause | Solution |
|---|---|---|
| `CMake introuvable` | PATH non coché | Réinstalle CMake en cochant « Add to PATH » |
| `Could not find MSBuild / cl.exe` | Charge de travail C++ non installée | Retourne dans Visual Studio Installer → Modifier → cocher « Desktop development with C++ » |
| Erreur réseau pendant `FetchContent` | Git bloqué par le pare-feu/proxy | Vérifier `git --version`, désactiver temporairement le proxy |
| `Access denied` à la copie | Droits admin | Copie manuelle (étape 5) |
| `The system cannot find the path specified` | Chemin avec accents/espaces | Déplace le projet vers `C:\Dev\autotune` |

---

## Ce que tu n'as PAS besoin de faire

- ❌ Installer JUCE à la main (le build s'en occupe)
- ❌ Installer le SDK VST3 (JUCE l'inclut)
- ❌ Acheter quoi que ce soit (licence GPLv3)
- ❌ Configurer Linux (c'est pour l'étape 16, plus tard)
