# Étape 1 — Diagnostic : de quoi tu disposes, et quel test tu peux faire

**But : ne pas te faire installer des choses inutiles, et savoir tout de suite quel test
est faisable chez toi.**

---

## 1. Lance le diagnostic (30 secondes)

1. Décompresse le ZIP `autotune-v0.1.zip`.
2. Va dans le dossier `autotune\outils\`.
3. **Double-clique sur `diagnostic.bat`.**
   *(Si Windows affiche un avertissement SmartScreen : « Informations complémentaires » →
   « Exécuter quand même ». Le script est en clair dans le fichier, tu peux l'ouvrir avec le
   Bloc-notes avant de le lancer.)*
4. Une fenêtre s'ouvre, puis le Bloc-notes affiche le rapport.
5. **Copie tout le contenu et envoie-le moi.**

Le rapport ne contient que des noms de logiciels et des versions — aucun fichier personnel.

---

## 2. Ce que le rapport va me dire, et ce que ça implique

| Ce que je vérifie | Si présent | Si absent |
|---|---|---|
| Windows 64 bits | ✅ on continue | ❌ problème bloquant |
| Visual Studio 2022 + charge C++ | ✅ Plan A : tu compiles chez toi | → Plan B (voir plus bas) |
| CMake | ✅ | à installer (5 min) |
| Git | ✅ | à installer (5 min) |
| Au moins un DAW | ✅ tu testes dans le DAW | → Plan C (version Standalone) |
| Dossier `C:\Program Files\Common Files\VST3` | ✅ | Windows le crée, je te donne la commande |
| ≥ 5 Go libres, ≥ 8 Go de RAM | ✅ | à surveiller |

---

## 3. Les trois plans possibles — je choisirai avec toi selon le rapport

### Plan A — Compilation locale (le plus rapide si VS est déjà là)
Tu installes CMake + Git si besoin, tu double-cliques `build.bat`, tu testes dans ton DAW.
→ **Temps : 15 à 40 min selon ce qui manque.**

### Plan B — Compilation par GitHub, sans rien installer
Si Visual Studio n'est pas installé et que tu ne veux pas télécharger 6 Go :

1. Tu crées un compte GitHub (gratuit) et un dépôt.
2. Tu y déposes le dossier `autotune` (le fichier `.github/workflows/build.yml` est déjà
   dedans — il est prêt).
3. GitHub compile automatiquement sur ses serveurs Windows.
4. Onglet **Actions** → dernier build → **Artifacts** → `HardTune-VST3-Windows`.
5. Tu récupères le `.vst3` déjà compilé et tu le copies dans
   `C:\Program Files\Common Files\VST3`.

→ **Temps : 10 min d'installation de rien du tout + 5 min de build.**

### Plan C — Version Standalone (si tu n'as pas de DAW)
Le build produit aussi **`HardTune.exe`**, une application autonome : tu choisis ton entrée
micro et ta sortie casque/sono dans l'appli, et tu testes sans DAW.
→ Il te faut quand même compiler (Plan A) ou passer par GitHub (Plan B).

---

## 4. Le test, adapté à chaque plan

### Variante 1 — Tu as un DAW (Plan A ou B)
Protocole : **`PROTOCOLE-DE-TEST.md`**, tests 1 à 6.
Tu insères le plugin sur une piste voix, preset T-PAIN, et tu écoutes.

### Variante 2 — Tu n'as pas de DAW (Plan C, version Standalone)
Protocole adapté, 5 minutes :

1. Lance **`HardTune.exe`** (produit par le build, dossier `Standalone`).
2. Clique sur **Options / Audio Settings** : choisis ton périphérique d'entrée
   (micro ou interface) et de sortie.
3. Clique sur **T-PAIN**.
4. Parle dans le micro, puis chante une gamme tenue.
5. Vérifie :
   - [ ] Tu t'entends (mets un casque pour éviter le larsen)
   - [ ] L'écran affiche la note que tu chantes
   - [ ] En chantant faux exprès, la voix est ramenée sur la note
   - [ ] RETUNE à 300 ms rend l'effet plus lent/naturel
6. Envoie-moi le même formulaire de réponses que pour la variante 1.

### Variante 3 — Tu n'as ni DAW ni Visual Studio
On fait le Plan B, puis la Variante 2 avec le `.exe` récupéré depuis GitHub.
C'est le chemin le plus court **si tu n'as rien installé**.

---

## 5. Ce que tu m'envoies (2 choses)

1. **Le contenu de `rapport-diagnostic.txt`.**
2. Rien d'autre pour l'instant. Je te réponds avec **le test exact à suivre**, en une seule
   liste numérotée, sans condition ni « si… sinon… ».

---

## Rappel des commandes, si tu préfères les taper à la main

Ouvre `cmd` (Menu Démarrer → tape `cmd` → Entrée) et colle ce bloc :

```bat
echo --- VERSION --- & ver & echo.
echo --- ARCHITECTURE --- & echo %PROCESSOR_ARCHITECTURE% & echo.
echo --- CMAKE --- & cmake --version 2>nul || echo CMake ABSENT & echo.
echo --- GIT --- & git --version 2>nul || echo Git ABSENT & echo.
echo --- VISUAL STUDIO --- & "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>nul || echo VS2022+C++ ABSENT & echo.
echo --- VST3 --- & dir "C:\Program Files\Common Files\VST3" /b 2>nul || echo Dossier VST3 ABSENT & echo.
echo --- DAW REAPER --- & if exist "%ProgramFiles%\REAPER\reaper.exe" (echo OK) else (echo ABSENT) & echo.
echo --- DAW FL --- & if exist "%ProgramFiles%\Image-Line" (echo OK) else (echo ABSENT) & echo.
echo --- DAW CUBASE --- & if exist "%ProgramFiles%\Steinberg" (echo OK) else (echo ABSENT) & echo.
echo --- DAW BITWIG --- & if exist "%ProgramFiles%\Bitwig Studio" (echo OK) else (echo ABSENT) & echo.
echo --- DISQUE --- & powershell -NoProfile -Command "[math]::Round((Get-PSDrive C).Free/1GB,1)" 
```

Copie la sortie et envoie-la : c'est équivalent au script.
