# HardTune v0.5.5 — correctif d'interface

## 1. Ce qui a été retiré (cause de la fermeture immédiate de la v0.5.4)

La v0.5.4 installait un **gestionnaire d'exceptions global** (`AddVectoredExceptionHandler`)
qui ouvrait un fichier à **chaque** access violation de tout le processus FL Studio
(loader Windows, overlays graphiques, plugins voisins…). Un tel gestionnaire peut
être rappelé pendant sa propre exécution : la pile explose et le DAW se ferme
instantanément, avant même d'afficher quoi que ce soit.

Il n'y a plus **aucun** handler global et plus **aucune** écriture disque pendant
le chargement du plugin.

## 2. Correctif côté interface

- **Accessibilité Windows (UIA) désactivée** : `setAccessible (false)` sur l'éditeur,
  ce qui couvre aussi tous les enfants. Nos libellés changent 30 fois par seconde ;
  chaque changement réveillait les clients UIA de la machine (overlays NVIDIA /
  Discord / Razer, Narrator, outils d'accessibilité), qui interrogeaient le plugin
  par messages de fenêtre — exactement la pile du plantage
  (`FLEngine → USER32 → HardTune.vst3`, `Read of address 0x0`).
  Un plugin de musique n'a rien à faire dans cet arbre.
- **Cadran** : le contour pointillé n'utilise plus `PathStrokeType::createDashedStroke`
  (code interne de JUCE appelé 30 fois par seconde). Ce sont maintenant de petits
  disques dessinés un par un : même aspect, plus de passage par le remplissage de
  chemins.
- Le fil qui construit l'éditeur est déclaré comme fil d'interface : **lui seul**
  écrit le journal de diagnostic (jamais le fil audio).

## 3. Le journal de diagnostic fonctionne maintenant vraiment

Fichier : `%TEMP%\hardtune-diag.txt` (Win+R → taper `%TEMP%` → Entrée).

Le fil d'interface y écrit **au plus 6 lignes par seconde** le nom de l'étape
courante. Si le plugin se ferme brutalement, **la dernière ligne du fichier
désigne la fonction exacte où le plugin se trouvait** :

| Dernière ligne | Où ça tombe |
|---|---|
| `paint:editeur` | le dessin général de la fenêtre |
| `paint:cadran` | le grand cadran central |
| `paint:oscilloscope` | la forme d'onde |
| `paint:vu-metre` | les deux vu-mètres |
| `paint:clavier` | le clavier |
| `resized` | la mise en page |
| `timer` | la lecture des valeurs (30 Hz) |
| `prepareToPlay` / `processBlock` | côté moteur audio |

## 4. Test de bissection : interface minimale

Si le plugin plante encore, créer un fichier vide nommé

```
%TEMP%\hardtune-ui-minimale.txt
```

puis recharger le plugin : il affiche une interface **minimale** (composants JUCE
standard uniquement, aucun dessin maison).

- Ça tient avec l'interface minimale → le défaut est dans **notre dessin**.
- Ça plante aussi avec l'interface minimale → le défaut est **ailleurs** (moteur,
  hôte, ou environnement).

Pour revenir à l'interface complète : supprimer ce fichier et recharger.

## 5. Ordre de test recommandé

1. Installer v0.5.5, ouvrir le plugin dans FL Studio → il doit s'afficher.
2. Jouer de l'audio 30 secondes → pas de fermeture.
3. Fermer puis réouvrir la fenêtre du plugin → pas de fermeture.
4. Fermer puis rouvrir le projet FL Studio → pas de fermeture.
5. En cas de fermeture : m'envoyer `%TEMP%\hardtune-diag.txt` (10 dernières lignes).
