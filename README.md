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

TuxAPE doit se trouver à côté, dans `../TuxAPE` (ou ailleurs avec
`-DTUXAPE_DIR=...`) : le plugin se compile contre son fichier
`sdk/plugin.h`, et avec le Qt 6 de TuxAPE. Il lui faut la version 3 de
l'interface des plugins, celle qui donne accès aux disquettes des lecteurs
et aux autres plugins.

```sh
cmake --preset dev
cmake --build build -j
```

L'archive à installer est alors dans `dist/` :
`tuxcmp-<version>-<plateforme>.zip`. Elle contient le dossier du plugin :
`plugin.json`, la bibliothèque et `bin/tuxcmp`.

## Installer

Dans TuxAPE : **Settings ▸ Plugins ▸ Install from ZIP...**, choisir l'archive,
confirmer. Le menu **Plugins** reçoit « TuxCMP – ZX0 Compressor... ».

Pour mettre les fichiers sur la disquette de TuxDSK, il faut une version de
TuxDSK qui les accepte (elle offre aux autres plugins `putFile()` et
`discName()`).

## Tests

```sh
ctest --preset dev
```

Les tests du compresseur (`zx0_*`) comparent ses sorties à celles du vrai
PackZX0 ; ceux du CPC (`cpc_*`) vérifient le loader, sa disposition en
mémoire et le programme Z80, exécuté sur un petit Z80 de test ; celui de la
ligne de commande (`cli_tuxcmp`) lance le programme ; ceux de l'interface
(`gui_compressor`, `plugin_window`) pilotent les fenêtres sans écran, avec un
TuxAPE et un TuxDSK factices.

`tools/cpc-check/run.sh` va plus loin, dans le CPC de TuxAPE (il faut TuxAPE
compilé, avec ses ROMs) : `DepackZX0.asm` assemblé par l'assembleur de
TuxAPE donne les octets de TuxCMP, un loader tapé dans le BASIC et sauvé par
lui donne les mêmes tokens, et des loaders lancés par `RUN` depuis une
disquette décompressent une image à l'écran, un programme qui est lancé, et
40 Ko qui chevauchent leurs données compressées.

## Organisation

Le projet est écrit en C++20. Les bibliothèques du moteur ne dépendent ni de
Qt ni du système.

| Dossier | Contenu |
|---|---|
| `src/base` | version |
| `src/zx0` | le compresseur ZX0, et une décompression qui fait ce que fait le Z80 |
| `src/cpc` | en-têtes AMSDOS, loader BASIC tokenisé, programme Z80, disposition en mémoire |
| `src/plugin` | interface Qt et lien avec TuxAPE et TuxDSK |
| `src/cli` | le programme `tuxcmp` |
| `tests` | programmes de test, et les écrans compressés par PackZX0 |
| `tools/cpc-check` | les loaders essayés dans le CPC de TuxAPE |

## Licence

PackZX0 est un gratuiciel de Ludovic Deplanque (Demoniak). ZX0 est d'Einar
Saukas, sa routine de décompression d'Einar Saukas et Urusergi, publiés sous
licence BSD à 3 clauses ; TuxCMP est distribué sous la même licence (voir
`LICENSE`).
