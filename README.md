# TuxCMP

Compresseur ZX0 pour Amstrad CPC, sous la forme d'un plugin de
[TuxAPE](../TuxAPE). C'est PackZX0 de Ludovic Deplanque (Demoniak) réécrit
en C++ : le même compresseur (ZX0 d'Einar Saukas), les mêmes fichiers
produits, octet pour octet, avec en plus de quoi les mettre sur une disquette
et les décompresser sur le CPC.

## Ce qu'il fait

La fenêtre s'ouvre par le menu **Plugins ▸ TuxCMP – ZX0 Compressor...**
(Ctrl+Maj+F8) ou par son bouton dans le panneau du bas.

1. **Le fichier** : n'importe quel fichier du PC, choisi par **Browse...** ou
   déposé sur la fenêtre. Il est compressé aussitôt, en arrière-plan (un
   écran de 16 Ko prend deux à trois secondes ; **Stop** arrête), puis
   décompressé à nouveau pour vérifier qu'il redonne bien le fichier. Un
   fichier qui a un en-tête AMSDOS est compressé sans lui (case à cocher).
2. **Le fichier compressé** : son nom sur la disquette (`SHOP.ZX0` pour
   `shop.scr`) et son en-tête AMSDOS, avec son adresse de chargement — il en
   faut un pour que `LOAD"SHOP.ZX0",&8E10` du BASIC le charge.
3. **Où il va** :

| Bouton | Ce qu'il fait |
|---|---|
| Put on TuxAPE's Disc | sur la disquette du lecteur choisi (A:, B:...) de TuxAPE ; la disquette est ensuite sauvée par TuxAPE comme celles où écrit AMSDOS |
| Put on TuxDSK's Disc | sur la première disquette de la fenêtre de TuxDSK (une est créée s'il n'y en a pas) ; le bouton n'est actif que si TuxDSK est installé et chargé, et la fenêtre dit sinon pourquoi |
| Save as File... | dans un fichier `.ZX0` du PC, comme le fait PackZX0 |

Un fichier du même nom déjà sur la disquette n'est remplacé qu'après
confirmation.

### Le programme de décompression

Une fois le fichier sur une disquette, TuxCMP demande : *« Do you want a
program made to decompress it? »*. Si oui, la fenêtre **Decompression
Program** prépare trois fichiers pour la même disquette :

- `SHOP.BAS`, un loader BASIC (déjà tokenisé, comme si on l'avait tapé) ;
- `SHOP.BIN`, le programme Z80 qu'il appelle : `DepackZX0` (la routine de
  Demoniak, 70 octets) précédé de `LD HL,source : LD DE,destination :
  CALL DepkZX0`, puis `RET` vers le BASIC ou `JP` vers le programme
  décompressé ;
- `SHOP.ZX0` à nouveau, son en-tête le chargeant là où le loader le charge.

```
10 REM SHOP.ZX0 to &C000, by TuxCMP
20 MODE 1
30 MEMORY &8DBF
40 LOAD"SHOP.ZX0",&8E10
50 LOAD"SHOP.BIN",&8DC0
60 CALL &8DC0
70 CALL &BB18
```

`RUN"SHOP"` le lance. On y règle où les données retournent (l'adresse de
l'en-tête du fichier d'origine s'il en avait un, `&C000` pour un écran,
`&4000` sinon), si elles sont ensuite lancées (son adresse d'exécution),
le `MODE` de départ, l'attente d'une touche à la fin (pour qu'une image reste
visible avant le `Ready` du BASIC), les noms et les adresses du programme et
du fichier compressé. **Place Automatically** les met là où rien ne se
chevauche ; **Show in the Assembler** ouvre la source du programme dans
l'assembleur de TuxAPE, **Save the Source...** l'écrit sur le PC. Sur une
disquette de TuxAPE, la case du bas tape `RUN"SHOP.BAS"` dans le CPC une fois
les fichiers posés.

À chaque modification, la décompression est essayée dans les 64 Ko du CPC,
instruction par instruction comme le Z80 la fera, et la fenêtre dit ce qui
n'irait pas ; le bouton **Put on the Disc** n'est actif que si tout va.

### Ce que le CPC permet

- Les fichiers que le loader charge vont de `&1300` à `&A67B` (sous le HIMEM
  d'AMSDOS), ou dans l'écran à partir de `&C000`. Sous `&1300`, `LOAD` répond
  *Memory full* : le BASIC veut 4 Ko libres au-dessus du loader pour son
  tampon de fichier (mesuré dans TuxAPE avec les ROMs du 6128).
- Les données décompressées ne peuvent pas aller de `&A67C` à `&BFFF`
  (AMSDOS, le firmware et la pile), ni sous `&0040`. Sous `&0400` elles
  recouvrent le loader lui-même : il faut alors qu'elles soient lancées.
- Le loader place d'abord le programme et le fichier compressé tout en haut,
  sous HIMEM ; sinon sous les données ; sinon après ; sinon le fichier
  compressé **chevauche la fin des données décompressées** : ZX0 le permet,
  pourvu qu'il se termine au moins `delta` octets après elles (`delta` est
  calculé par le compresseur) ; le programme va alors là où il reste de la
  place, dans l'écran au besoin.

## Fidélité à PackZX0

Le format est celui de PackZX0 et de sa routine `DepackZX0` : le premier
format de ZX0, que ses versions suivantes appellent « classique »
(`zx0 -c`), compressé de façon optimale (sans `-q`). Les tests le vérifient
sur des écrans compressés par le vrai PackZX0 (`tests/golden`) : TuxCMP
produit les mêmes octets.

PackZX0 n'a pas pu être lancé ici (il lui faut .NET, absent de wine) : ce
qu'il met dans l'en-tête d'un fichier compressé dont l'original en avait un
vient de ses messages (*« Amsdos header detected, will write Amsdos header to
destination file »*), et `tuxcmp` y met le type et les adresses de
l'original, avec la longueur compressée.

## Le programme en ligne de commande

`tuxcmp` fait ce que fait `PackZX0.exe` :

```sh
tuxcmp SHOP.SCR                    # SHOP.SCR.ZX0
tuxcmp -o SHOP.ZX0 SHOP.SCR        # sous un autre nom
tuxcmp -d SHOP.SCR.ZX0             # décompresse : SHOP.SCR
```

Il est dans l'archive du plugin, sous `bin/` : une fois le plugin installé,
`plugins/tuxcmp/bin/tuxcmp`. Il ne dépend pas de Qt.

## Compiler

TuxCMP suit les conventions des plugins de TuxAPE (`PLUGINS.md`, à la racine
de TuxAPE) et ne prend de TuxAPE que son dossier `sdk/` : `plugin.h` et le
module CMake `TuxAPEPlugin.cmake`. TuxAPE doit se trouver à côté, dans
`../TuxAPE`, ou ailleurs avec `-DTUXAPE_SDK=/chemin/TuxAPE/sdk`. Il lui faut
la version 4 de l'interface des plugins : les disquettes des lecteurs et les
autres plugins (version 3), la langue de TuxAPE (version 4).

```sh
cmake --preset dev          # Qt du système, ou celui du .deps/ de TuxAPE
cmake --build build -j
ctest --preset dev
```

L'archive à installer est alors dans `dist/` :
`tuxcmp-<version>-<plateforme>.zip`. Elle contient le dossier du plugin :
`plugin.json`, la bibliothèque, `bin/tuxcmp` et `LICENSE`, et
`signature.json` quand la clé de l'auteur est nommée :

```sh
cmake -S . -B build -DTUXAPE_SIGN_KEY=$HOME/.config/tuxape-signing/moi.key
cmake --build build
```

Sans clé nommée, l'archive n'est pas signée : compiler TuxCMP ne le signe
jamais au nom de quelqu'un par mégarde.

## Installer

Dans TuxAPE : **Settings ▸ Plugins ▸ Install from ZIP...** (**Réglages ▸
Plug-ins** en français), choisir l'archive, confirmer. Le menu **Plugins**
reçoit « TuxCMP – ZX0 Compressor... ».

Pour mettre les fichiers sur la disquette de TuxDSK, il faut une version de
TuxDSK qui les accepte (elle offre aux autres plugins `putFile()` et
`discName()`).

## Les langues

TuxCMP parle la langue de TuxAPE : anglais, la langue de ses sources, ou
français. Sa traduction est dans `translations/tuxcmp_fr.ts`, compilée dans
la bibliothèque et chargée au démarrage du plugin.

Les sources disent les pluriels avec « (s) » (`%n file(s)`) ; l'anglais a ses
formes, « 1 file », « 3 files », dans `translations/tuxcmp_en.ts`, qui ne
contient qu'elles.

Après un changement des textes :

```sh
cmake --build build --target tuxcmp_update_translations   # lupdate
linguist translations/tuxcmp_fr.ts                         # traduire ce qui est nouveau
linguist translations/tuxcmp_en.ts                         # et les pluriels anglais
cmake --build build
```

Le programme en ligne de commande, lui, écrit en anglais, comme PackZX0.

## Tests

```sh
ctest --preset dev
```

Les tests du compresseur (`engine_zx0_*`) comparent ses sorties à celles du
vrai PackZX0 ; ceux du CPC (`engine_cpc_*`) vérifient l'en-tête AMSDOS, le
loader, sa disposition en mémoire et le programme Z80, exécuté sur un petit
Z80 de test ; celui de la ligne de commande (`cli_tuxcmp`) lance le
programme ; `plugin_window` charge le plugin comme TuxAPE le charge, en
anglais, en français et dans une langue qu'il n'a pas ; `gui_compressor`
pilote les fenêtres sans écran, avec un TuxAPE et un TuxDSK factices.

## Organisation

Le projet est écrit en C++20. Le moteur ne dépend ni de Qt, ni de TuxAPE, ni
du système, et n'a aucun texte pour l'utilisateur : les fenêtres disent ce
qu'il rend.

| Dossier | Contenu |
|---|---|
| `src/engine/base` | version |
| `src/engine/zx0` | le compresseur ZX0, et une décompression qui fait ce que fait le Z80 |
| `src/engine/cpc` | en-têtes AMSDOS, loader BASIC tokenisé, programme Z80, disposition en mémoire |
| `src/plugin` | les fenêtres (`tuxcmp_ui`), le lien avec TuxAPE et TuxDSK, la classe chargée (`tuxcmp`), tout dans `namespace tuxcmp` |
| `src/cli` | le programme `tuxcmp` |
| `translations` | la traduction française |
| `tests` | programmes de test, et les écrans compressés par PackZX0 |

## Licence

PackZX0 est un gratuiciel de Ludovic Deplanque (Demoniak). ZX0 est d'Einar
Saukas, sa routine de décompression d'Einar Saukas et Urusergi, publiés sous
licence BSD à 3 clauses ; TuxCMP est distribué sous la même licence (voir
`LICENSE`).
